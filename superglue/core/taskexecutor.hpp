#ifndef __TASKEXECUTOR_HPP__
#define __TASKEXECUTOR_HPP__

template<typename Options> class TaskQueue;
template<typename Options> class ThreadManager;
template<typename Options> class TaskExecutor;

namespace detail {

// ===========================================================================
// Option SubTasks
// ===========================================================================
template<typename Options, typename T = typename Options::SubTasks> struct TaskExecutor_SubTasks;

template<typename Options>
struct TaskExecutor_SubTasks<Options, typename Options::Disable> {};

template<typename Options>
struct TaskExecutor_SubTasks<Options, typename Options::Enable> {

    static bool checkDependencies(TaskBase<Options> *task, typename Options::LazyDependencyChecking) {
        return true;
    }
    static bool checkDependencies(TaskBase<Options> *task, typename Options::EagerDependencyChecking) {
        return task->areDependenciesSolvedOrNotify();
    }

    // called to create task from a task.
    // task gets assigned to calling task executor
    // task added to front of queue
    void addSubTask(TaskBase<Options> *task) {
        TaskExecutor<Options> *this_((TaskExecutor<Options> *)(this));

        if (!checkDependencies(task, typename Options::DependencyChecking()))
            return;

        this_->getTaskQueue().push_front(task);
    }
};

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

        setStolen(task, typename Options::TaskStolenFlag());
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
// Option CurrentTask: Remember current task
// ============================================================================
template<typename Options, typename T = typename Options::ExecutorCurrentTask> class TaskExecutor_CurrentTask;

template<typename Options>
class TaskExecutor_CurrentTask<Options, typename Options::Disable> {
public:
    void rememberTask(TaskBase<Options> *) const {}
};

template<typename Options>
class TaskExecutor_CurrentTask<Options, typename Options::Enable> {
private:
    TaskBase<Options> *currentTask;
public:
    TaskExecutor_CurrentTask() : currentTask(0) {}

    void rememberTask(TaskBase<Options> *task) { currentTask = task; }
    TaskBase<Options> *getCurrentTask() { return currentTask; }
};

} // namespace detail

// ============================================================================
// TaskExecutor
// ============================================================================
template<typename Options>
class TaskExecutor
  : public detail::TaskExecutor_CurrentTask<Options>,
    public detail::TaskExecutor_GetThreadWorkspace<Options>,
    public detail::TaskExecutor_PassTaskExecutor<Options>,
    public detail::TaskExecutor_Stealing<Options>,
    public detail::TaskExecutor_SubTasks<Options>,
    public Options::TaskExecutorInstrumentation
{
    template<typename, typename> friend struct TaskExecutor_Stealing;
    template<typename, typename> friend struct TaskExecutor_PassThreadId;

public:
    const int id;
    ThreadManager<Options> &tm;
    char padding1[64];
    TaskQueue<Options> readyList;
    char padding2[64];

    // Called from this thread only
    TaskBase<Options> *getTaskInternal() {
        TaskExecutor<Options> *this_(static_cast<TaskExecutor<Options> *>(this));

        TaskBase<Options> *task = 0;
        if (this_->readyList.get(task))
            return task;

        return detail::TaskExecutor_Stealing<Options>::steal();
    }

    void invokeTask(TaskBase<Options> *task) {

        Options::TaskExecutorInstrumentation::runTaskBefore(task);

        detail::TaskExecutor_PassTaskExecutor<Options>::invokeTaskImpl(task);

        Options::TaskExecutorInstrumentation::runTaskAfter(task);

        finished(task);
    }

    void finished(TaskBase<Options> *task) {
        const size_t numAccess = task->getNumAccess();
        Access<Options> *access(task->getAccess());
        for (size_t i = numAccess; i > 0; --i) {
            if (access[i-1].useContrib())
                access[i-1].getHandle()->increaseCurrentVersionNoUnlock(*this);
            else
                access[i-1].getHandle()->increaseCurrentVersion(*this);
        }
        Options::FreeTask::free(task);
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
        const size_t numAccess = task->getNumAccess();
        Access<Options> *access(task->getAccess());
        for (size_t i = 0; i < numAccess; ++i) {
            if (!access[i].getLockOrNotify(task)) {
                for (size_t j = 0; j < i; ++j)
                    access[i-j-1].releaseLock(*this);
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

public:

    typename Options::UserThreadData userThreadData;

    TaskExecutor(int id_, ThreadManager<Options> &tm_)
      : id(id_), tm(tm_)
    {
        Options::TaskExecutorInstrumentation::init(*this);
        this->init_stealing(this);
    }

    ~TaskExecutor() {
        Options::TaskExecutorInstrumentation::destroy();
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
        detail::TaskExecutor_CurrentTask<Options>::rememberTask(task);
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

#endif // __TASKEXECUTOR_HPP__
