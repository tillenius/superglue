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
    size_t barrier_counter; // written by everybody, read by main thread
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

    // Called from TaskExecutor: Wait at barrier if requested.
    void wait_at_barrier(TaskExecutor<Options> &te) {
        Atomic::compiler_fence();
        const int local_state(state);
        if (local_state == 0)
            return;

        if (te.my_barrier_state == local_state)
            return;

        te.my_barrier_state = local_state;

        // new state

        if (local_state == 1) {
            // enter barrier
            if (Atomic::decrease_nv(&barrier_counter) != 0) {
                // wait for next state
                for (;;) {
                    const int local_abort(abort);
                    if (local_abort == 1) {
                        // aborted from elsewhere -- skip spinloop and start workloop
                        return;
                    }

                    if (!te.get_task_queue().empty()) {
                        abort = 1;
                        Atomic::memory_fence_producer();
                        // we have to abort -- indicate others and start workloop
                        return; // work loop, state == 1
                    }

                    const int new_local_state(state);
                    if (new_local_state == 2) {
                        // completed phase 1 without seeing abort. continue to phase 2
                        break;
                    }
                    Atomic::yield();
                    Atomic::compiler_fence();
                }
                if (!te.get_task_queue().empty_safe()) {
                    abort = 1;
                    Atomic::memory_fence_producer();
                    // we have to abort -- indicate others and start workloop
                    return; // work loop, state == 1
                }
                // phase 1 completed without abort.
                state = 2;

                te.my_barrier_state = 2;
                if (Atomic::decrease_nv(&barrier_counter) == 0) {
                    // last into phase 2

                    barrier_counter = tm.get_num_cpus()-2;
                    te.my_barrier_state = 0;
                    const int local_abort(abort);
                    if (local_abort != 1) {
                        if (!te.get_task_queue().empty_safe()) {
                            // we have to abort -- start next phase and exit to work-loop
                            abort = 1;
                            Atomic::memory_fence_producer();
                        }
                    }
                    // leave barrier
                    state = 0;
                    return;
                }
                // phase 2, but not last. fall-through
            }
            else {
                // last in for state 1 -- start phase 2
                const size_t num_workers = tm.get_num_cpus()-1;
                if (num_workers == 1) {
                    // if there is a single worker we can't wait for anybody else to finish the barrier.
                    if (!te.get_task_queue().empty_safe()) {
                        abort = 1;
                        Atomic::memory_fence_producer();
                    }
                    te.my_barrier_state = 0;
                    state = 0;
                    return;
                }

                const int local_abort(abort);
                barrier_counter = num_workers-1;
                te.my_barrier_state = 2;
                if (local_abort == 1) {
                    // already aborted -- start next phase and exit to work-loop
                    state = 2;
                    return;
                }
                if (!te.get_task_queue().empty_safe()) {
                    // we have to abort -- start next phase and exit to work-loop
                    abort = 1;
                    Atomic::memory_fence_producer();
                    state = 2;
                    return;
                }
                // start next phase
                state = 2;
            }

            // in phase 2, but not last -- wait for completion
            for (;;) {
                const int local_abort(abort);
                if (local_abort == 1) {
                    // aborted from elsewhere -- skip spinloop and start workloop
                    return;
                }

                if (!te.get_task_queue().empty()) {
                    abort = 1;
                    Atomic::memory_fence_producer();
                    // we have to abort -- indicate others and start workloop
                    return;
                }

                const int new_local_state(state);
                if (new_local_state != 2) {
                    // completed phase 2 without seeing abort. finish barrier
                    return;
                }
                Atomic::yield();
                Atomic::compiler_fence();
            }
        }

        // first time we see state == 2, and barrier is already aborted

        if (Atomic::decrease_nv(&barrier_counter) == 0) {
            // last into phase 2 -- finish barrier
            te.my_barrier_state = 0;
            state = 0;
        }

        return;
    }

    // cannot be invoked by more than one thread at a time
    void barrier(TaskExecutor<Options> &te) {
        tm.start_executing();

        {
            TaskQueueUnsafe woken;
            while (te.execute_tasks(woken));
        }

        const size_t num_workers(tm.get_num_cpus()-1);

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
