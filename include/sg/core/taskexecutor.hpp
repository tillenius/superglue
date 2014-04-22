#ifndef SG_TASKEXECUTOR_HPP_INCLUDED
#define SG_TASKEXECUTOR_HPP_INCLUDED

#include "sg/platform/atomic.hpp"
#include <iostream>
#include <cstdlib> // exit()
#include <cstdio> // exit()

namespace sg {

template<typename Options> class TaskBase;
template<typename Options> class Access;
template<typename Options> class SuperGlue;
template<typename Options> class TaskExecutor;

namespace detail {

// ============================================================================
// Option NoStealing
// ============================================================================
template<typename Options, typename T = typename Options::Stealing> class TaskExecutor_Stealing;

template<typename Options>
class TaskExecutor_Stealing<Options, typename Options::Disable> {
public:
    static TaskBase<Options> *steal() { return 0; }
    static void init_stealing() {}
};

template<typename Options>
class TaskExecutor_Stealing<Options, typename Options::Enable> {
    typename Options::StealOrder stealorder;
public:

    void init_stealing() {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        stealorder.init(this_);
    }

    TaskBase<Options> *steal() {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        TaskBase<Options> *task = 0;
        if (!stealorder.steal(this_->get_threading_manager(), this_->get_id(), task))
            return 0;

        return task;
    }
};

// ============================================================================
// Option TaskExecutor_PassTaskExecutor: Task called with task executor as argument
// ============================================================================
template<typename Options, typename T = typename Options::PassTaskExecutor> class TaskExecutor_PassTaskExecutor;

template<typename Options>
class TaskExecutor_PassTaskExecutor<Options, typename Options::Disable> {
protected:
    static void invoke_task_impl(TaskBase<Options> *task) { task->run(); }
};
template<typename Options>
class TaskExecutor_PassTaskExecutor<Options, typename Options::Enable> {
protected:
    void invoke_task_impl(TaskBase<Options> *task) {
        task->run(static_cast<TaskExecutor<Options> &>(*this));
    }
};

// ===========================================================================
// Option ThreadWorkspace
// ===========================================================================
template<typename Options, typename T = typename Options::ThreadWorkspace> class TaskExecutor_GetThreadWorkspace;

template<typename Options>
class TaskExecutor_GetThreadWorkspace<Options, typename Options::Disable> {
public:
    void reset_workspace() const {}
};

template<typename Options>
class TaskExecutor_GetThreadWorkspace<Options, typename Options::Enable> {
private:
    typename Options::template Alloc<char>::type allocator;
    char *workspace;
    size_t index;

public:
    TaskExecutor_GetThreadWorkspace() : index(0) {
        workspace = allocator.allocate(Options::ThreadWorkspace_size);
    }
    ~TaskExecutor_GetThreadWorkspace() {
        allocator.deallocate(workspace, Options::ThreadWorkspace_size);
    }
    void reset_workspace() { index = 0; }

    void *get_thread_workspace(const size_t size) {
        if (index > Options::ThreadWorkspace_size) {
            std::cerr
                << "### out of workspace. allocated=" << index << "/" << Options::ThreadWorkspace_size
                << ", requested=" << size << std::endl;
            ::exit(-1);
        }
        void *res = &workspace[index];
        index += size;
        return res;
    }
};

// ============================================================================
// Option Subtasks
// ============================================================================
template<typename Options, typename T = typename Options::Subtasks> class TaskExecutor_Subtasks;

template<typename Options>
class TaskExecutor_Subtasks<Options, typename Options::Disable> {
    typedef typename Options::ReadyListType TaskQueue;
    typedef typename TaskQueue::unsafe_t TaskQueueUnsafe;
public:
    void finished(TaskBase<Options> *task, TaskQueueUnsafe &woken) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        this_->release_task(task, woken);
    }
};

template<typename Options>
class TaskExecutor_Subtasks<Options, typename Options::Enable> {
    typedef typename Options::ReadyListType TaskQueue;
    typedef typename TaskQueue::unsafe_t TaskQueueUnsafe;
public:
    void subtask(TaskBase<Options> *parent, TaskBase<Options> *task) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));

        if (parent->subtask_count == 0)
            parent->subtask_count = 2;
        else
            Atomic::increase(&parent->subtask_count);
        task->parent = parent;
        this_->submit_front(task);
    }

    void finished(TaskBase<Options> *task, TaskQueueUnsafe &woken) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));

        if (task->subtask_count != 0) {
            size_t nv = Atomic::decrease_nv(&task->subtask_count);
            if (nv != 0)
                return;
        }

        this_->release_task(task, woken);

        TaskBase<Options> *current_parent = task->parent;
        while (current_parent != NULL) {
            // this task is a subtask of some other task

            if (Atomic::decrease_nv(&current_parent->subtask_count) > 0)
                break;

            // release parent and continue to parent's parent
            TaskBase<Options> *next_parent = current_parent->parent;
            this_->release_task(current_parent, woken);
            current_parent = next_parent;
        }

    }
};

} // namespace detail

// ============================================================================
// TaskExecutorBase
// ============================================================================
template<typename Options>
class TaskExecutorBase
  : public detail::TaskExecutor_GetThreadWorkspace<Options>,
    public detail::TaskExecutor_PassTaskExecutor<Options>,
    public detail::TaskExecutor_Stealing<Options>,
    public detail::TaskExecutor_Subtasks<Options>,
    public Options::Instrumentation
{
    template<typename, typename> friend class TaskExecutor_Stealing;
    template<typename, typename> friend class TaskExecutor_PassThreadId;
    typedef typename Options::ThreadingManagerType ThreadingManager;
    typedef typename Options::version_t version_t;
    typedef typename Options::ReadyListType TaskQueue;
    typedef typename TaskQueue::unsafe_t TaskQueueUnsafe;

private:
    TaskExecutorBase(const TaskExecutorBase &);
    const TaskExecutorBase &operator=(const TaskExecutorBase &);


public:
    const int id;
    ThreadingManager &tman;
    char padding1[Options::CACHE_LINE_SIZE];
    TaskQueue ready_list;
    char padding2[Options::CACHE_LINE_SIZE];

    // Called from this thread only
    TaskBase<Options> *get_task_internal() {
        for (;;) {
            TaskBase<Options> *task = 0;
            if (ready_list.pop_front(task)) {

                // TODO: Can we hide this somewhere?
                const int location = task->get_location();
                if (location != -1 && location != id) {
                    tman.get_task_queues()[location]->push_front(task);
                    continue;
                }

                return task;
            }

	        return detail::TaskExecutor_Stealing<Options>::steal();
        }
    }

    void invoke_task(TaskBase<Options> *task, TaskQueueUnsafe &woken) {
        // book-keeping
        detail::TaskExecutor_GetThreadWorkspace<Options>::reset_workspace();
        apply_old_contribs(task);

        // push woken tasks to the ready queue, so other threads can steal while we work
        if (!woken.empty()) {
            push_front_list(woken);
            TaskQueueUnsafe().swap(woken);
        }

        Options::Instrumentation::run_task_before(task);

        detail::TaskExecutor_PassTaskExecutor<Options>::invoke_task_impl(task);

        Options::Instrumentation::run_task_after(task);

        detail::TaskExecutor_Subtasks<Options>::finished(task, woken);
    }

    bool run_contrib(TaskBase<Options> *task) {
        if (!task->can_run_with_contribs())
            return false;

        const size_t num_access = task->get_num_access();
        Access<Options> *access(task->get_access());
        for (size_t i = 0; i < num_access; ++i) {
            if (!access[i].get_lock())
                access[i].set_use_contrib(true);
        }
        return true;
    }

    void apply_old_contribs(TaskBase<Options> *task) {
        const size_t num_access = task->get_num_access();
        Access<Options> *access(task->get_access());
        for (size_t i = 0; i < num_access; ++i) {
            if (!access[i].needs_lock())
                access[i].get_handle()->apply_old_contribs();
        }
    }

    bool try_lock(TaskBase<Options> *task, TaskQueueUnsafe &woken) {

        const size_t num_access = task->get_num_access();
        Access<Options> *access(task->get_access());
        for (size_t i = 0; i < num_access; ++i) {
            if (!access[i].get_lock_or_notify(task)) {
                for (size_t j = 0; j < i; ++j)
                    access[i-j-1].release_lock(woken);
                return false;
            }
        }
        return true;
    }

    void release_task(TaskBase<Options> *task, TaskQueueUnsafe &woken) {
        const size_t num_access = task->get_num_access();
        Access<Options> *access(task->get_access());
        for (size_t i = num_access; i > 0; --i) {
            version_t ver = access[i-1].finished(woken);
            Options::LogDAG::task_finish(task, access[i-1].get_handle(), ver);
        }
        Options::FreeTask::free(task);
    }

    void submit_front(TaskBase<Options> *task) {
        if (!Options::DependencyChecking::check_at_submit(task))
            return;

        ready_list.push_front(task);
        tman.barrier_protocol.signal_new_work();
    }

    void push_front_list(TaskQueueUnsafe &wake) {
        ready_list.push_front_list(wake);
        tman.barrier_protocol.signal_new_work();
    }

public:
    bool terminate_flag;
    int my_barrier_state;

    TaskExecutorBase(int id_, ThreadingManager &tman_)
      : Options::Instrumentation(id_), id(id_), tman(tman_),
        terminate_flag(false), my_barrier_state(0)
    {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        this_->init_stealing();
    }

    // Called from this thread only
    bool execute_tasks(TaskQueueUnsafe &woken) {
        for (;;) {
            TaskBase<Options> *task;

            if (!woken.pop_front(task)) {
                task = get_task_internal();
                if (task == 0) {
                    Atomic::yield();
                    return false;
                }
            }

            if (!Options::DependencyChecking::check_before_execution(task))
                continue;

            // run with contributions, if that is activated
            if (run_contrib(task)) {
                invoke_task(task, woken);
                return true;
            }

            // lock if needed
            if (!try_lock(task, woken))
                continue;

            // run
            invoke_task(task, woken);
            return true;
        }
    }


    // Called from other threads
    void terminate() {
        terminate_flag = true;
    }

    void work_loop() {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        for (;;) {
            {
                TaskQueueUnsafe woken;
                while (execute_tasks(woken));
            }

            tman.barrier_protocol.wait_at_barrier(*this_);

            if (terminate_flag)
                return;

            Atomic::rep_nop();
        }
    }

    void submit(TaskBase<Options> *task) {
        if (!Options::DependencyChecking::check_at_submit(task))
            return;

        ready_list.push_back(task);
        tman.barrier_protocol.signal_new_work();
    }

    TaskQueue &get_task_queue() { return ready_list; }
    int get_id() const { return id; }
    ThreadingManager &get_threading_manager() { return tman; }
};

// export Options::TaskExecutorType as TaskExecutor (default: TaskExecutorBase<Options>)
template<typename Options> class TaskExecutor : public Options::TaskExecutorType {
public:
    TaskExecutor(int id, typename Options::ThreadingManagerType &tman)
    : Options::TaskExecutorType(id, tman) {}
};

} // namespace sg

#endif // SG_TASKEXECUTOR_HPP_INCLUDED
