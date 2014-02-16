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
template <typename Options> class WorkerThread;
template <typename Options> class ThreadManager;

namespace detail {

// ===========================================================================
// Option PauseExecution
// ===========================================================================
template<typename Options, typename T = typename Options::PauseExecution> class ThreadManager_PauseExecution;

template<typename Options>
class ThreadManager_PauseExecution<Options, typename Options::Disable> {
public:
    static bool mayExecute() { return true; }
};

template<typename Options>
class ThreadManager_PauseExecution<Options, typename Options::Enable> {
    char dummy[Options::HardwareModel::CACHE_LINE_SIZE];
    bool flag;
    char dummy2[Options::HardwareModel::CACHE_LINE_SIZE];
public:
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
    enum { access_types_needs_lockable = 1 };
};

template<typename Options>
struct CheckLockableRequired<Options, typename Options::Disable> {
    template<typename T>
    struct NeedsLockablePredicate {
        enum { result = ((T::exclusive == 1) && (T::commutative == 1)) ? 1 : 0 };
    };

    typedef AccessUtil<Options> AU;
    typedef typename AU::template AnyType<NeedsLockablePredicate> NeedsLock;

    enum { access_types_needs_lockable = NeedsLock::result ? 0 : 1};
};

template<bool> struct STATIC_ASSERT {};
template<> struct STATIC_ASSERT<true> { typedef struct {} type; };

template<typename Options>
struct SANITY_CHECKS {

    template<typename T>
    struct is_unsigned {
        enum { value = T(-1) > T(0) ? 1 : 0 };
    };

    // version_t must be an unsigned type
    typedef typename STATIC_ASSERT< is_unsigned<typename Options::version_t>::value >::type check_version_t;

    // check that Lockable isn't disabled when access types require it to be enabled
    typedef typename STATIC_ASSERT< CheckLockableRequired<Options>::access_types_needs_lockable >::type check_lockable;
};

} // namespace detail

// ===========================================================================
// ThreadManagerBase
// ===========================================================================
template<typename Options>
class ThreadManagerBase
  : public detail::ThreadManager_PauseExecution<Options>,
    private detail::SANITY_CHECKS<Options>
{
    template<typename> friend class ThreadManager_GetCurrentThread;
    template<typename, typename> friend class ThreadManager_GetThreadWorkspace;
    template<typename, typename> friend class ThreadManager_PauseExecution;
    typedef typename Options::ReadyListType TaskQueue;

private:
    const int numWorkers;

    ThreadManagerBase(const ThreadManagerBase &);
    ThreadManagerBase &operator=(const ThreadManagerBase &);

public:
    BarrierProtocol<Options> barrierProtocol;
    char padding[Options::HardwareModel::CACHE_LINE_SIZE];
    Barrier *startBarrier; // only used during thread creation
    WorkerThread<Options> **threads;
    TaskQueue **taskQueues;


    // called from thread starter. Can be called by many threads concurrently
    void registerThread(int id, WorkerThread<Options> *wt) {
        threads[id-1] = wt;
        taskQueues[id] = &wt->getTaskQueue();
        taskQueues[id]->init_thread(id);
        Log<Options>::registerThread(id);
    }

    // ===========================================================================
    // WorkerThreadStarter: Helper thread to start Worker thread
    // ===========================================================================
    template<typename Ops>
    class WorkerThreadStarter : public Thread {
    private:
        const int id;
        ThreadManager<Ops> &tm;

    public:
        WorkerThreadStarter(int id_, ThreadManager<Ops> &tm_)
        : id(id_), tm(tm_) {}

        void run() {
            // pin thread to specific cpu core
            Options::ThreadAffinity::pin_workerthread(id);

            // allocate Worker on thread
            WorkerThread<Ops> *wt = new WorkerThread<Ops>(id, tm);
            tm.registerThread(id, wt);
            wt->run(this);
        }
    };

public:
    ThreadManagerBase(int numWorkers_ = -1)
      : numWorkers(numWorkers_ == -1 ? decideNumWorkers() : numWorkers_),
        barrierProtocol(*static_cast<ThreadManager<Options> *>(this))
    {
        ThreadManager<Options> *this_(static_cast<ThreadManager<Options> *>(this));
        assert(numWorkers_ == -1 || numWorkers_ >= 0);

        Options::HardwareModel::init();
        Options::ThreadAffinity::pin_main_thread();
        taskQueues = new TaskQueue *[getNumQueues()];
        taskQueues[0] = &barrierProtocol.getTaskQueue();
        taskQueues[0]->init(getNumQueues());

        threads = new WorkerThread<Options>*[numWorkers];
        startBarrier = new Barrier(numWorkers+1);
        Log<Options>::init();
        Log<Options>::registerThread(0); // register main thread
        for (int i = 0; i < numWorkers; ++i) {
            WorkerThreadStarter<Options> *wts =
                new WorkerThreadStarter<Options>(i+1, *this_);
            wts->start();
        }

        // waiting on a pthread barrier here instead of using
        // atomic increases made the new threads start much faster
        startBarrier->wait();

        // Cannot destroy startbarrier
        // unless we are certain that we are the last one into
        // the barrier, which we cannot know. it is not enough
        // that everyone has reached the barrier to destroy it
    }

    ~ThreadManagerBase() {
        assert(detail::ThreadManager_PauseExecution<Options>::mayExecute());
        barrier();

        for (int i = 0; i < numWorkers; ++i)
            threads[i]->setTerminateFlag();

        for (int i = 0; i < numWorkers; ++i)
            threads[i]->join();

        delete startBarrier;
        for (int i = 0; i < numWorkers; ++i)
            delete threads[i];

        delete[] threads;

        Options::TaskExecutorInstrumentation::finalize();
    }

    int getNumWorkers() const { return numWorkers; }
    TaskQueue **getTaskQueues() const { return taskQueues; }
    int getNumQueues() const { return numWorkers+1; }
    WorkerThread<Options> *getWorker(int i) { return threads[i]; }

    static int decideNumWorkers() {
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

// export Options::ThreadManagerType as ThreadManager (default: ThreadManagerBase<Options>)
template<typename Options> class ThreadManager : public Options::ThreadManagerType {
public:
    ThreadManager(int num_workers = -1) : Options::ThreadManagerType(num_workers) {}
};

#endif // __THREADMANAGER_HPP__
