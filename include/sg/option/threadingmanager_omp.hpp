#ifndef SG_THREADINGMANAGER_OMP_HPP_INCLUDED
#define SG_THREADINGMANAGER_OMP_HPP_INCLUDED

#include "sg/platform/openmputil.hpp"
#include "sg/platform/atomic.hpp"
#include "sg/core/spinlock.hpp"
#include <cassert>

namespace sg {

template <typename Options> class BarrierProtocol;
template <typename Options> class SuperGlue;
template <typename Options> class TaskExecutor;

template <typename Options>
class ThreadingManagerOMP {
    typedef typename Options::ReadyListType TaskQueue;
    typedef typename Options::ThreadingManagerType ThreadingManager;

private:
    SpinLock startup_lock;
    SpinLock lock_workers_initialized;

    int start_counter;
    char padding1[Options::CACHE_LINE_SIZE];
    int num_cpus;

public:
    BarrierProtocol<Options> barrier_protocol;
    TaskExecutor<Options> **threads;
    TaskQueue **task_queues;


private:
    static bool workers_start_paused(typename Options::Disable) { return false; }
    static bool workers_start_paused(typename Options::Enable) { return true; }
    static bool workers_start_paused() { return workers_start_paused(typename Options::PauseExecution()); }

    static int decide_num_cpus() { return omp_get_num_threads(); }
    static int get_thread_num() { return omp_get_thread_num(); }

    void init_master() {
        Options::ThreadAffinity::pin_main_thread();
        num_cpus = decide_num_cpus();
        threads = new TaskExecutor<Options> *[num_cpus];
        task_queues = new TaskQueue*[num_cpus];

        threads[0] = new TaskExecutor<Options>(0, *this);
        task_queues[0] = &threads[0]->get_task_queue();

        startup_lock.unlock();

        while (start_counter != num_cpus)
            Atomic::rep_nop();

        if (!workers_start_paused())
            lock_workers_initialized.unlock();
    }

    void init_worker(int id) {
        Options::ThreadAffinity::pin_workerthread(id);
        // allocate Worker on thread
        TaskExecutor<Options> *te = new TaskExecutor<Options>(id, *this);

        // wait until main thread has initialized the threadmanager
        startup_lock.lock();
        startup_lock.unlock();

        threads[id] = te;
        task_queues[id] = &te->get_task_queue();

        Atomic::increase(&start_counter);

        lock_workers_initialized.lock();
        lock_workers_initialized.unlock();

        te->work_loop();
    }

public:
    ThreadingManagerOMP(int req = -1)
    : start_counter(1),
      barrier_protocol(*static_cast<ThreadingManager*>(this))
    {
        assert(req == -1);

        // In the OpenMP backend, the ThreadManager is instantiated
        // in a serial section, so in contrast to the threaded backend
        // we do not know the number of threads here.
        // Instead we find out when we are called again in thread_main()
        startup_lock.lock();
        lock_workers_initialized.lock();

        Options::ThreadAffinity::init();
    }

    void init() {
        const int id(get_thread_num());
        if (id == 0)
            init_master();
        else
            init_worker(id);
    }

    void stop() {
        if (get_thread_num() != 0) {
            
            // workers don't come here until terminate() has been called

            int nv = Atomic::decrease_nv(&start_counter);

            // wait until all workers reached this step
            // all threads must agree that we are shutting
            // down before we can continue and invoke the
            // destructor
            startup_lock.lock();
            startup_lock.unlock();
            return;
        }

        start_executing(); // make sure threads have been started, or we will wait forever in barrier
        barrier_protocol.barrier(*threads[0]);

        startup_lock.lock();

        for (int i = 1; i < get_num_cpus(); ++i)
            threads[i]->terminate();


        // wait for all threads to join
        while (start_counter != 1)
            Atomic::rep_nop();

        // signal that threads can destruct
        startup_lock.unlock();

        for (int i = 1; i < get_num_cpus(); ++i)
            delete threads[i];

        delete [] threads;
        delete [] task_queues;
    }

    void start_executing() {
        if (workers_start_paused()) {
            if (lock_workers_initialized.is_locked())
                lock_workers_initialized.unlock();
        }
    }

    TaskQueue **get_task_queues() const { return const_cast<TaskQueue**>(&task_queues[0]); }
    TaskExecutor<Options> *get_worker(int i) { return threads[i]; }
    int get_num_cpus() { return num_cpus; }
};

} // namespace sg

#endif // SG_THREADINGMANAGER_OMP_HPP_INCLUDED

