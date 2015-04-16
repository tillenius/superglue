#ifndef SG_BARRIERPROTOCOL_HPP_INCLUDED
#define SG_BARRIERPROTOCOL_HPP_INCLUDED

#include "sg/platform/atomic.hpp"

namespace sg {

template <typename Options> class TaskExecutor;

template <typename Options>
class BarrierProtocol {
private:
    typedef typename Options::ThreadingManagerType ThreadingManager;
    typedef typename Options::ReadyListType TaskQueue;
    typedef typename TaskQueue::unsafe_t TaskQueueUnsafe;

    ThreadingManager &tm;
    char padding1[Options::CACHE_LINE_SIZE];
    unsigned int barrier_counter; // written by everybody, read by main thread
    char padding2[Options::CACHE_LINE_SIZE];
    int state;             // written by anyone, 3 times per try, read by everybody
    int abort;             // read/written on every task submit. written by anyone, 1 time per try, read by main thread.
    char padding3[Options::CACHE_LINE_SIZE];

private:

public:
    BarrierProtocol(ThreadingManager &tm_)
      : tm(tm_), barrier_counter(0), state(0), abort(1)
    {
    }

    // Called from TaskExecutor: return true if we are allowed to run tasks
    bool update_barrier_state(TaskExecutor<Options> &te) {
        Atomic::compiler_fence();
        const int local_state(state);

        // return if not in barrier
        if (local_state == 0)
            return true;

        // set abort flag if we have tasks
        if (!te.get_task_queue().empty()) {
            if (abort != 1) {
                abort = 1;
                Atomic::memory_fence_producer(); // make sure abort is visible before state changes
            }
        }

        // return if barrier is in same state as last time
        if (te.my_barrier_state == local_state)
            return abort == 1;

        // new state

        te.my_barrier_state = local_state;

        // enter barrier
        const unsigned int local_barrier_counter(Atomic::decrease_nv(&barrier_counter));

        // return if not last
        if (local_barrier_counter != 0)
            return abort == 1;

        // we are last to enter the barrier

        if (local_state == 1) {
            const unsigned int num_workers = tm.get_num_cpus() - 1;
            // if single worker, the barrier is finished
            if (num_workers == 1) {
                te.my_barrier_state = 0;
                state = 0;
                return true;
            }

            // setup state 2, join it, and return
            te.my_barrier_state = 2;
            barrier_counter = num_workers - 1;
            Atomic::memory_fence_producer(); // make sure barrier_counter is visible before state changes
            state = 2;
            return abort == 1;
        }

        // last in for stage 2 -- finish barrier
        te.my_barrier_state = 0;
        state = 0;
        return true;
    }

    // cannot be invoked by more than one thread at a time
    void barrier(TaskExecutor<Options> &te) {
        tm.start_executing();

        {
            TaskQueueUnsafe woken;
            while (te.execute_tasks(woken));
        }

        const unsigned int num_workers(static_cast<unsigned int>(tm.get_num_cpus())-1);

        if (num_workers == 0)
            return;

        for (;;) {
            {
                TaskQueueUnsafe woken;
                while (te.execute_tasks(woken));
            }

            barrier_counter = num_workers;
            abort = 0;
            Atomic::memory_fence_producer();
            state = 1;

            for (;;) {
                const int local_state(state);
                if (local_state == 0) {
                    Atomic::memory_fence_consumer();
                    const int local_abort(abort);
                    if (local_abort == 1)
                        break;
                    if (!te.get_task_queue().empty_safe())
                        break;
                    return;
                }

                const int local_abort(abort);
                if (local_abort == 1 || !te.get_task_queue().empty_safe()) {
                    while (state != 0) {

                        {
                            TaskQueueUnsafe woken;
                            while (te.execute_tasks(woken));
                        }

                        Atomic::compiler_fence();
                    }
                    break;
                }
                Atomic::rep_nop();
            }
        }
    }

    void signal_new_work() {
        Atomic::compiler_fence();
        const int local_abort(abort);
        if (local_abort != 1) {
            abort = 1;
            Atomic::memory_fence_producer();
        }
    }
};

} // namespace sg

#endif // SG_BARRIERPROTOCOL_HPP_INCLUDED
