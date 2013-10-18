#ifndef __THREADMANAGER_HPP__
#define __THREADMANAGER_HPP__

// ThreadManager
//
// Creates and owns the worker threads and task pools.

#include "platform/threads.hpp"
#include "platform/mutex.hpp"
#include "platform/barrier.hpp"
#include "core/barrierprotocol.hpp"
#include "core/log.hpp"

#include <cassert>

template <typename Options> class TaskBase;
template <typename Options> class TaskQueue;
template <typename Options> class WorkerThread;
template <typename Options> class ThreadManager;

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
    char dummy[64];
    bool flag;
    char dummy2[64];
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
    char padding[64];
    Barrier *startBarrier; // only used during thread creation
    WorkerThread<Options> **threads;
    TaskQueue<Options> **taskQueues;

    ThreadManager(const ThreadManager &);
    ThreadManager &operator=(const ThreadManager &);

    // called from thread starter. Can be called by many threads concurrently
    void registerThread(int id, WorkerThread<Options> *wt) {
        threads[id-1] = wt;
        taskQueues[id] = &wt->getTaskQueue();
        Log<Options>::registerThread(id);
    }

    // ===========================================================================
    // WorkerThreadStarter: Helper thread to start Worker thread
    // ===========================================================================
    template<typename Ops>
    class WorkerThreadStarter : public Thread {
    private:
        const int id;
        ThreadManager &tm;

    public:
        WorkerThreadStarter(int id_, ThreadManager &tm_)
        : id(id_), tm(tm_) {}

        void run() {
            // allocate Worker on thread
            WorkerThread<Ops> *wt = new WorkerThread<Ops>(id, tm);
            tm.registerThread(id, wt);
            wt->run(this);
        }
    };


public:
    ThreadManager(int numWorkers_ = -1)
      : numWorkers(numWorkers_ == -1 ? decideNumWorkers() : numWorkers_),
        barrierProtocol(*this)
    {
        assert(numWorkers_ == -1 || numWorkers_ >= 0);

        Options::HardwareModel::init();
        ThreadUtil::setAffinity(Options::HardwareModel::cpumap(0));
        taskQueues = new TaskQueue<Options> *[getNumQueues()];
        taskQueues[0] = &barrierProtocol.getTaskQueue();

        threads = new WorkerThread<Options>*[numWorkers];
        startBarrier = new Barrier(numWorkers+1);
        Log<Options>::init(numWorkers+1);
        for (size_t i = 0; i < numWorkers; ++i) {
            WorkerThreadStarter<Options> *wts =
                new WorkerThreadStarter<Options>(i+1, *this);
            wts->start(Options::HardwareModel::cpumap(i+1));
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

        for (size_t i = 0; i < numWorkers; ++i)
            threads[i]->join();

        delete startBarrier;
        for (size_t i = 0; i < numWorkers; ++i)
            delete threads[i];

        delete[] threads;
    }

    size_t getNumWorkers() const { return numWorkers; }
    TaskQueue<Options> **getTaskQueues() const { return taskQueues; }
    size_t getNumQueues() const { return numWorkers+1; }
    WorkerThread<Options> *getWorker(size_t i) { return threads[i]; }

    static size_t decideNumWorkers() {
        const char *var = getenv("OMP_NUM_THREADS");
        if (var != NULL) {
            const int OMP_NUM_THREADS(atoi(var));
			assert(OMP_NUM_THREADS >= 0);
            if (OMP_NUM_THREADS != 0)
                return OMP_NUM_THREADS-1;
        }
        return ThreadUtil::getNumCPUs()-1;
    }

    void signalNewWork() { barrierProtocol.signalNewWork(); }

    // called from worker threads during startup
    void waitToStartThread() {
        startBarrier->wait();
        while (!detail::ThreadManager_PauseExecution<Options>::mayExecute())
            Atomic::rep_nop(); // include force reload the mayExecute-flag
    }

    // USER INTERFACE {

    void submit(TaskBase<Options> *task) {
        barrierProtocol.submit(task);
        barrierProtocol.signalNewWork();
    }

    void submit(TaskBase<Options> *task, int cpuid) {
        if (cpuid == 0)
            barrierProtocol.submit(task);
        else
            getWorker(cpuid-1)->submit(task);
        barrierProtocol.signalNewWork();
    }

    void barrier() {
        barrierProtocol.barrier();
    }

    void wait(HandleBase<Options> *handle) {
        while (handle->nextVersion()-1 != handle->getCurrentVersion()) {
            barrierProtocol.executeTasks();
            Atomic::rep_nop(); // to reload the handle versions
        }
    }

    // }
};

#endif // __THREADMANAGER_HPP__
