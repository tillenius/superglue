#ifndef SG_THREADINGMANAGER_DEFAULT_HPP_INCLUDED
#define SG_THREADINGMANAGER_DEFAULT_HPP_INCLUDED

#include "sg/platform/threads.hpp"
#include "sg/platform/threadutil.hpp"
#include "sg/platform/atomic.hpp"
#include "sg/core/spinlock.hpp"

#include <vector>
#include <cstdio>

namespace sg {

template <typename Options> class SuperGlue;
template <typename Options> class BarrierProtocol;
template <typename Options> class TaskExecutor;

template <typename Options>
class ThreadingManagerDefault {
    typedef typename Options::ReadyListType TaskQueue;
    typedef typename Options::ThreadingManagerType ThreadingManager;

private:
    // ===========================================================================
    // WorkerThread: Thread to run worker
    // ===========================================================================
    class WorkerThread : public Thread {
    private:
        const int id;
        ThreadingManagerDefault &tman;

    public:
        WorkerThread(int id_, ThreadingManagerDefault &tman_)
        : id(id_), tman(tman_) {}

        void run() {
            Options::ThreadAffinity::pin_workerthread(id);

            // allocate Worker on thread
            TaskExecutor<Options> *te = new TaskExecutor<Options>(id, tman);

            tman.threads[id] = te;
            tman.task_queues[id] = &te->get_task_queue();

            Atomic::increase(&tman.start_counter);

            tman.lock_workers_initialized.lock();
            tman.lock_workers_initialized.unlock();

            te->work_loop();
        }
    };

    SpinLock lock_workers_initialized;
    int start_counter;
    char padding1[Options::CACHE_LINE_SIZE];
    int num_cpus;
    std::vector<WorkerThread *> workerthreads;

public:
    BarrierProtocol<Options> barrier_protocol;
    TaskExecutor<Options> **threads;
    TaskQueue **task_queues;

private:
    static bool workers_start_paused(typename Options::Disable) { return false; }
    static bool workers_start_paused(typename Options::Enable) { return true; }
    static bool workers_start_paused() { return workers_start_paused(typename Options::PauseExecution()); }

    int decide_num_cpus(int requested) {
        assert(requested == -1 || requested > 0);
        const char *var = getenv("OMP_NUM_THREADS");
        if (var != NULL) {
            const int OMP_NUM_THREADS(atoi(var));
            assert(OMP_NUM_THREADS >= 0);
            if (OMP_NUM_THREADS != 0)
                return OMP_NUM_THREADS;
        }
        if (requested == -1 || requested == 0)
            return ThreadUtil::get_num_cpus();
        return requested;
    }

public:
    ThreadingManagerDefault(int requested_num_cpus = -1)
    : start_counter(1),
      num_cpus(decide_num_cpus(requested_num_cpus)),
      barrier_protocol(*static_cast<ThreadingManager*>(this))
    {
        Options::ThreadAffinity::init();
        Options::ThreadAffinity::pin_main_thread();

        lock_workers_initialized.lock();
        threads = new TaskExecutor<Options> *[num_cpus];
        task_queues = new TaskQueue*[num_cpus];

        threads[0] = new TaskExecutor<Options>(0, *this);
        task_queues[0] = &threads[0]->get_task_queue();

        const int num_workers(num_cpus-1);
        workerthreads.resize(num_workers);
        for (int i = 0; i < num_workers; ++i) {
            workerthreads[i] = new WorkerThread(i+1, *this);
            workerthreads[i]->start();
        }

        while (start_counter != num_cpus)
            Atomic::rep_nop();

        if (!workers_start_paused())
            lock_workers_initialized.unlock();
    }

    void stop() {
        start_executing(); // make sure threads have been started, or we will wait forever in barrier
        barrier_protocol.barrier(*threads[0]);

        for (int i = 1; i < get_num_cpus(); ++i)
            threads[i]->terminate();

        const int num_workers(num_cpus-1);
        for (int i = 0; i < num_workers; ++i)
            workerthreads[i]->join();
        for (int i = 0; i < num_workers; ++i)
            delete workerthreads[i];
        for (int i = 0; i < num_cpus; ++i)
            delete threads[i];

        delete [] threads;
        delete [] task_queues;
    }

    void start_executing() {
        if (workers_start_paused() && lock_workers_initialized.is_locked())
            lock_workers_initialized.unlock();
    }

    TaskQueue **get_task_queues() const { return const_cast<TaskQueue**>(&task_queues[0]); }
    TaskExecutor<Options> *get_worker(int i) { return threads[i]; }
    int get_num_cpus() { return num_cpus; }
};

} // namespace sg

#endif // SG_THREADINGMANAGER_DEFAULT_HPP_INCLUDED
