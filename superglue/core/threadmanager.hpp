#ifndef __THREADMANAGER_HPP__
#define __THREADMANAGER_HPP__

// ThreadManager
//
// Creates and owns the worker threads and task pools.

#include "platform/threads.hpp"
#include "core/barrierprotocol.hpp"

template <typename Options> class TaskBase;
template <typename Options> class TaskQueue;
template <typename Options> class WorkerThread;
template <typename Options> class ThreadManager;
template <typename Options> class Log;

namespace detail {

// ===========================================================================
// Option PauseExecution
// ===========================================================================
template<typename Options, typename T = typename Options::PauseExecution> struct ThreadManager_PauseExecution;

template<typename Options>
struct ThreadManager_PauseExecution<Options, typename Options::Disable> {
    static bool mayExecute() { return true; }
};

template<typename Options>
struct ThreadManager_PauseExecution<Options, typename Options::Enable> {
    bool flag;
    ThreadManager_PauseExecution() : flag(false) {}
    void setMayExecute(bool flag_) { flag = flag_; }
    bool mayExecute() const { return flag; }
};

// ===========================================================================
// CheckLockableRequired -- check that no access types commutes and required
// exclusive access if lockable is disabled
// ===========================================================================
template<typename Options, typename T = typename Options::Lockable>
struct CheckLockableRequired {
    typedef T type;
};

template<typename Options>
struct CheckLockableRequired<Options, typename Options::Disable> {
    typedef typename Options::AccessInfoType AccessInfo;

    template<typename T>
    struct NeedsLockablePredicate {
        enum { result = ((T::exclusive == 1) && (T::commutative == 1)) ? 1 : 0 };
    };

    // struct that typedefs T if n == 0.
    template<typename T, int n> struct if_zero {};
    template<typename T> struct if_zero<T, 0>
    {
        typedef T CONFIGURATION_ERROR___ACCESS_TYPES_NEEDS_LOCK_BUT_LOCKABLE_NOT_SET;
    };

    typedef AccessUtil<Options> AU;
    typedef typename AU::template AnyType<NeedsLockablePredicate> NeedsLock;
    enum { needs_lock = NeedsLock::result };
    typedef typename if_zero<AccessInfo, needs_lock>::CONFIGURATION_ERROR___ACCESS_TYPES_NEEDS_LOCK_BUT_LOCKABLE_NOT_SET type;
};


} // namespace detail

// ===========================================================================
// ThreadManager
// ===========================================================================
template<typename Options>
class ThreadManager
  : public detail::ThreadManager_PauseExecution<Options>
{
    template<typename> friend struct ThreadManager_GetCurrentThread;
    template<typename, typename> friend struct ThreadManager_GetThreadWorkspace;
    template<typename, typename> friend struct ThreadManager_PauseExecution;
    typedef typename detail::CheckLockableRequired<Options>::type ACCESSINFOTYPE;

private:
    const size_t numWorkers;

public:
    BarrierProtocol<Options> barrierProtocol;
    Barrier *startBarrier; // only used during thread creation
    WorkerThread<Options> **threads;
    TaskQueue<Options> **taskQueues;

    ThreadManager(const ThreadManager &);
    ThreadManager &operator=(const ThreadManager &);

    void registerTaskQueues() const {
       ThreadManager<Options> *this_((ThreadManager<Options> *)(this));
       this_->taskQueues[getNumQueues()-1] = &this_->barrierProtocol.getTaskQueue();
   }

    // called from thread starter. Can be called by many threads concurrently
    void registerThread(int id, WorkerThread<Options> *wt) {
        threads[id] = wt;
        taskQueues[id] = &wt->getTaskQueue();
        Log<Options>::registerThread(id+1);
    }

    // ===========================================================================
    // WorkerThreadStarter: Helper thread to start Worker thread
    // ===========================================================================
    template<typename Ops>
    class WorkerThreadStarter : public Thread {
    private:
        int id;
        ThreadManager &tm;

    public:
        WorkerThreadStarter(int id_,
                            ThreadManager &tm_)
        : id(id_), tm(tm_) {}

        void run() {
            // allocate Worker on thread
            WorkerThread<Ops> *wt = new WorkerThread<Ops>(id, tm);
            tm.registerThread(id, wt);
            wt->run(this);
        }
    };

    static bool dependenciesReadyAtSubmit(TaskBase<Options> *task, typename Options::EagerDependencyChecking) {
        return task->areDependenciesSolvedOrNotify();
    }

    static bool dependenciesReadyAtSubmit(TaskBase<Options> *, typename Options::LazyDependencyChecking) {
        return true;
    }

    // submits a task. One is required to invoke signalNewWork() after tasks have been submitted,
    // but it is enough to do this once after submitting several tasks.
    void submitNoSignal(TaskBase<Options> *task, int cpuid) {
        if (!dependenciesReadyAtSubmit(task, typename Options::DependencyChecking()))
            return;

        const size_t queueIndex = Options::Scheduler::place(task, cpuid, getNumQueues());
        taskQueues[queueIndex]->push_back(task);
    }

public:
    ThreadManager(int numWorkers_ = -1)
      : numWorkers(numWorkers_ == -1 ? (ThreadUtil::getNumCPUs()-1) : numWorkers_),
        barrierProtocol(*this)
    {
        ThreadUtil::setAffinity(Options::HardwareModel::cpumap(ThreadUtil::getNumCPUs()-1)); // force master thread to run on last cpu.
        taskQueues = new TaskQueue<Options> *[getNumQueues()];
        registerTaskQueues();

        threads = new WorkerThread<Options>*[numWorkers];
        startBarrier = new Barrier(numWorkers+1);
        Log<Options>::init(numWorkers+1);
        for (size_t i = 0; i < numWorkers; ++i) {
            WorkerThreadStarter<Options> *wts =
                    new WorkerThreadStarter<Options>(i, *this);
            wts->start(Options::HardwareModel::cpumap(i));
        }

        // waiting on a pthread barrier here instead of using
        // atomic increases made the new threads start much faster
        startBarrier->wait();

        // Cannot destroy startbarrier
        // unless we are certain that we are the last one into
        // the barrier, which we cannot know. it is not enough
        // that everyone has reached the barrier to destroy it
    }

    ~ThreadManager() {
        assert(detail::ThreadManager_PauseExecution<Options>::mayExecute());
        barrier();

        for (size_t i = 0; i < numWorkers; ++i)
            threads[i]->setTerminateFlag();

        barrierProtocol.signalNewWork();

        for (size_t i = 0; i < numWorkers; ++i)
            threads[i]->join();

        delete startBarrier;
        for (size_t i = 0; i < numWorkers; ++i)
            delete threads[i];

        delete[] threads;
    }

    size_t getNumWorkers() const { return numWorkers; }
    TaskQueue<Options> **getTaskQueues() const { return taskQueues; }
    static size_t getNumQueues(const size_t numWorkers) { return numWorkers+1; }
    size_t getNumQueues() const { return getNumQueues(numWorkers); }
    WorkerThread<Options> *getWorker(size_t i) { return threads[i]; }

    // INTERFACE TO HANDLE (SUBMITS TASKS OF THEIR OWN) {

    void signalNewWork() { barrierProtocol.signalNewWork(); }

    // }

    // called from worker threads during startup
    void waitToStartThread() {
        startBarrier->wait();
        while (!detail::ThreadManager_PauseExecution<Options>::mayExecute())
            Atomic::compiler_fence(); // to reload the mayExecute-flag
    }

    // USER INTERFACE {

    WorkerThread<Options> *getCurrentThread() const {
        const ThreadManager<Options> *this_((const ThreadManager<Options> *)(this));
        const ThreadIDType t(ThreadUtil::getCurrentThreadId());
        for (size_t i = 0; i < this_->numWorkers; ++i)
            if (this_->threads[i]->getThread()->getThreadId() == t)
                return this_->threads[i];
        std::cerr << "getCurrentThread() failed" << std::endl;
        exit(1);
        return 0;
    }

    void submit(TaskBase<Options> *task, int cpuid = -1) {
        submitNoSignal(task, cpuid);
        barrierProtocol.signalNewWork();
    }

    void barrier() {
        barrierProtocol.barrier();
    }

    void wait(Handle<Options> *handle) {
        while (handle->nextVersion()-1 != handle->getCurrentVersion()) {
            TaskExecutor<Options>::executeTasks();
            Atomic::compiler_fence(); // to reload the handle versions
        }
    }

    static size_t suggestnumWorkers() { return ThreadUtil::getNumCPUs()-1; }
    // }
};

#endif // __THREADMANAGER_HPP__
