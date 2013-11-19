#ifndef __TASKEXECUTOR_HPP__
#define __TASKEXECUTOR_HPP__

#include "core/log_dag.hpp"

template<typename Options> class TaskQueue;
template<typename Options> class ThreadManager;
template<typename Options> class TaskExecutor;

namespace detail {

// ============================================================================
// Option NoStealing
// ============================================================================
template<typename Options, typename T = typename Options::Stealing> struct TaskExecutor_Stealing;

template<typename Options>
struct TaskExecutor_Stealing<Options, typename Options::Disable> {
    static TaskBase<Options> *steal() { return 0; }
    static void init_stealing(TaskExecutor<Options> *) {}
};

template<typename Options>
struct TaskExecutor_Stealing<Options, typename Options::Enable> {
    typename Options::StealOrder stealorder;

    void init_stealing(TaskExecutor<Options> *te) {
        stealorder.init(te);
    }

    static void setStolen(TaskBase<Options> *, typename Options::Disable) {};
    static void setStolen(TaskBase<Options> *task, typename Options::Enable) {
        task->setStolen(true);
    };

    TaskBase<Options> *steal() {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        TaskBase<Options> *task = 0;
        if (!stealorder.steal(this_->getThreadManager(), this_->getId(), task))
            return 0;

        return task;
    }
};

// ============================================================================
// Option TaskExecutor_PassTaskExecutor: Task called with task executor as argument
// ============================================================================
template<typename Options, typename T = typename Options::PassTaskExecutor> struct TaskExecutor_PassTaskExecutor;

template<typename Options>
struct TaskExecutor_PassTaskExecutor<Options, typename Options::Disable> {
    static void invokeTaskImpl(TaskBase<Options> *task) { task->run(); }
};
template<typename Options>
struct TaskExecutor_PassTaskExecutor<Options, typename Options::Enable> {
    void invokeTaskImpl(TaskBase<Options> *task) {
        task->run(static_cast<TaskExecutor<Options> &>(*this));
    }
};

// ===========================================================================
// Option ThreadWorkspace
// ===========================================================================
template<typename Options, typename T = typename Options::ThreadWorkspace> class TaskExecutor_GetThreadWorkspace;

template<typename Options>
struct TaskExecutor_GetThreadWorkspace<Options, typename Options::Disable> {
    void resetWorkspaceIndex() const {}
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
    void resetWorkspaceIndex() { index = 0; }

    void *getThreadWorkspace(const size_t size) {
        if (index > Options::ThreadWorkspace_size) {
            std::cerr
                << "### out of workspace. allocated=" << index << "/" << Options::ThreadWorkspace_size
                << ", requested=" << size << std::endl;
            exit(-1);
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
public:
    void finished(TaskBase<Options> *task) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        this_->release_task(task);
    }
};

template<typename Options>
class TaskExecutor_Subtasks<Options, typename Options::Enable> {
public:
    void subtask(TaskBase<Options> *parent, TaskBase<Options> *task) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));

        if (parent->subtask_count == 0)
            parent->subtask_count = 2;
        else
            Atomic::increase(&parent->subtask_count);
        task->parent = parent;
        this_->submit(task);
    }

    void finished(TaskBase<Options> *task) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));

        if (task->subtask_count == 0) {
            // no subtasks, we release the task immediately
            this_->release_task(task);
            return;
        }

        TaskBase<Options> *current_parent = task->parent;
        while (current_parent != NULL) {
            // this task is a subtask of some other task

            if (Atomic::decrease_nv(&current_parent->subtask_count) > 0)
                break;

            // release parent and continue to parent's parent
            TaskBase<Options> *next_parent = current_parent->parent;
            this_->release_task(current_parent);
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
    public Options::TaskExecutorInstrumentation
{
    template<typename, typename> friend struct TaskExecutor_Stealing;
    template<typename, typename> friend struct TaskExecutor_PassThreadId;

public:
    const int id;
    ThreadManager<Options> &tm;
    char padding1[Options::HardwareModel::CACHE_LINE_SIZE];
    TaskQueue<Options> readyList;
    char padding2[Options::HardwareModel::CACHE_LINE_SIZE];

    // Called from this thread only
    TaskBase<Options> *getTaskInternal() {
        TaskBase<Options> *task = 0;
        if (readyList.pop_front(task))
            return task;

        return detail::TaskExecutor_Stealing<Options>::steal();
    }

    void invokeTask(TaskBase<Options> *task) {

        Options::TaskExecutorInstrumentation::runTaskBefore(task);

        detail::TaskExecutor_PassTaskExecutor<Options>::invokeTaskImpl(task);

        Options::TaskExecutorInstrumentation::runTaskAfter(task);

        detail::TaskExecutor_Subtasks<Options>::finished(task);
    }

    static bool dependenciesReadyAtExecution(TaskBase<Options> *task, typename Options::LazyDependencyChecking) {
        return task->areDependenciesSolvedOrNotify();
    }
    static bool dependenciesReadyAtExecution(TaskBase<Options> *task, typename Options::EagerDependencyChecking) {
        return true;
    }

    bool runContrib(TaskBase<Options> *task) {
        if (!task->canRunWithContribs())
            return false;

        const size_t numAccess = task->getNumAccess();
        Access<Options> *access(task->getAccess());
        for (size_t i = 0; i < numAccess; ++i) {
            if (!access[i].getLock())
                access[i].setUseContrib(true);
        }
        return true;
    }

    void applyOldContributionsBeforeRead(TaskBase<Options> *task) {
        const size_t numAccess = task->getNumAccess();
        Access<Options> *access(task->getAccess());
        for (size_t i = 0; i < numAccess; ++i) {
            if (!access[i].needsLock())
                access[i].getHandle()->applyOldContributionsBeforeRead();
        }
    }

    bool tryLock(TaskBase<Options> *task) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));

        const size_t numAccess = task->getNumAccess();
        Access<Options> *access(task->getAccess());
        for (size_t i = 0; i < numAccess; ++i) {
            if (!access[i].getLockOrNotify(task)) {
                for (size_t j = 0; j < i; ++j)
                    access[i-j-1].releaseLock(*this_);
                return false;
            }
        }
        return true;
    }

    static bool dependenciesReadyAtSubmit(TaskBase<Options> *task, typename Options::EagerDependencyChecking) {
        return task->areDependenciesSolvedOrNotify();
    }

    static bool dependenciesReadyAtSubmit(TaskBase<Options> *, typename Options::LazyDependencyChecking) {
        return true;
    }

    void release_task(TaskBase<Options> *task) {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));

        const size_t numAccess = task->getNumAccess();
        Access<Options> *access(task->getAccess());
        for (size_t i = numAccess; i > 0; --i) {
            size_t ver;
            if (access[i-1].useContrib())
                ver = access[i-1].getHandle()->increaseCurrentVersionNoUnlock(*this_);
            else
                ver = access[i-1].getHandle()->increaseCurrentVersion(*this_);
            Log_DAG<Options>::dag_taskFinish(task, access[i-1].getHandle(), ver);
        }
        Options::FreeTask::free(task);
    }

public:

    TaskExecutorBase(int id_, ThreadManager<Options> &tm_)
      : id(id_), tm(tm_)
    {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        Options::TaskExecutorInstrumentation::init(*this_);
        this_->init_stealing(this_);
    }

    ~TaskExecutorBase() {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));
        Options::TaskExecutorInstrumentation::destroy(*this_);
    }

    // Called from this thread only
    bool executeTasks() {
        TaskBase<Options> *task = getTaskInternal();
        if (task == 0)
            return false;

        if (!dependenciesReadyAtExecution(task, typename Options::DependencyChecking())) {
            Options::TaskExecutorInstrumentation::taskNotRunDeps();
            return false;
        }

        // book-keeping
        detail::TaskExecutor_GetThreadWorkspace<Options>::resetWorkspaceIndex();
        applyOldContributionsBeforeRead(task);

        // run with contributions, if that is activated
        if (runContrib(task)) {
            invokeTask(task);
            return true;
        }

        // run, lock if needed
        if (tryLock(task)) {
            invokeTask(task);
            return true;
        }
        else {
            Options::TaskExecutorInstrumentation::taskNotRunLock();
        }
        return false;
    }

    void submit(TaskBase<Options> *task) {
        if (!dependenciesReadyAtSubmit(task, typename Options::DependencyChecking()))
            return;

        readyList.push_back(task);
        tm.signalNewWork();
    }

    TaskQueue<Options> &getTaskQueue() { return readyList; }
    int getId() const { return id; }
    ThreadManager<Options> &getThreadManager() { return tm; }
};

// export Options::TaskExecutorType as TaskExecutor (default: TaskExecutorBase<Options>)
template<typename Options> class TaskExecutor : public Options::TaskExecutorType {
public:
    TaskExecutor(int id, ThreadManager<Options> &tm)
    : Options::TaskExecutorType(id, tm) {}
};

#endif // __TASKEXECUTOR_HPP__
