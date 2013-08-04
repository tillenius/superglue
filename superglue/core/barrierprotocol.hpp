#ifndef __BARRIERPROTOCOL_HPP__
#define __BARRIERPROTOCOL_HPP__

#include "platform/atomic.hpp"
#include "core/log.hpp"

template <typename Options> class ThreadManager;
template <typename Options> class WorkerThread;

template <typename Options>
class BarrierProtocol
  : public TaskExecutor<Options>
{
private:
    char padding1[64];
    size_t barrierCounter; // written by everybody, read by main thread
    char padding2[64];
    int state;             // written by anyone, 3 times per try, read by everybody
    int abort;             // written by anyone, 1 time per try, read by main thread

private:

public:
    BarrierProtocol(ThreadManager<Options> &tm_)
      : TaskExecutor<Options>(0, tm_),
        barrierCounter(0), state(0), abort(1)
    {
    }

    // Called from WorkerThread: Wait at barrier if requested.
    void waitAtBarrier(WorkerThread<Options> &wwt) {
        const int local_state(*static_cast<volatile int *>(&state));
        if (local_state == 0)
            return;

        if (wwt.my_barrier_state == local_state)
            return;

        wwt.my_barrier_state = local_state;

        // new state

        if (local_state == 1) {
            // enter barrier
            if (Atomic::decrease_nv(&barrierCounter) != 0) {
                // wait for next state
                for (;;) {
                    const int local_abort(*static_cast<volatile int *>(&abort));
                    if (local_abort == 1) {
                        // aborted from elsewhere -- skip spinloop and start workloop
                        return;
                    }

                    if (wwt.getTaskQueue().gotWorkSafe()) {
                        abort = 1;
                        Atomic::memory_fence_producer();
                        // we have to abort -- indicate others and start workloop
                        return; // work loop, state == 1
                    }

                    const int new_local_state(*static_cast<volatile int *>(&state));
                    if (new_local_state == 2) {
                        // completed phase 1 without seeing abort. continue to phase 2
                        break;
                    }
                }
                // phase 1 completed without abort.
                state = 2;

                wwt.my_barrier_state = 2;
                if (Atomic::decrease_nv(&barrierCounter) == 0) {
                    // last into phase 2

                    barrierCounter = TaskExecutor<Options>::getThreadManager().getNumWorkers()-1;
                    wwt.my_barrier_state = 0;
                    const int local_abort(*static_cast<volatile int *>(&abort));
                    if (local_abort != 1) {
                        if (wwt.getTaskQueue().gotWorkSafe()) {
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
                const size_t num_workers = TaskExecutor<Options>::getThreadManager().getNumWorkers();
                if (num_workers == 1) {
                    // if there is a single worker we can't wait for anybody else to finish the barrier.
                    wwt.my_barrier_state = 2;
                    state = 0;
                    //Atomic::memory_fence_producer();
                    return;
                }

                const int local_abort(*static_cast<volatile int *>(&abort));
                barrierCounter = num_workers-1;
                wwt.my_barrier_state = 2;
                if (local_abort == 1) {
                    // already aborted -- start next phase and exit to work-loop
                    state = 2;
                    return;
                }
                if (wwt.getTaskQueue().gotWorkSafe()) {
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
                const int local_abort(*static_cast<volatile int *>(&abort));
                if (local_abort == 1) {
                    // aborted from elsewhere -- skip spinloop and start workloop
                    return;
                }

                if (wwt.getTaskQueue().gotWorkSafe()) {
                    abort = 1;
                    Atomic::memory_fence_producer();
                    // we have to abort -- indicate others and start workloop
                    return;
                }

                const int new_local_state(*static_cast<volatile int *>(&state));
                if (new_local_state != 2) {
                    // completed phase 2 without seeing abort. finish barrier
                    return;
                }
            }
        }

        // first time we see state == 2, and barrier is already aborted

        if (Atomic::decrease_nv(&barrierCounter) == 0) {
            // last into phase 2 -- finish barrier
            wwt.my_barrier_state = 0;
            state = 0;
        }

        return;
    }

    // cannot be invoked by more than one thread at a time
    void barrier() {
        while (TaskExecutor<Options>::executeTasks());

        const size_t numWorkers(TaskExecutor<Options>::getThreadManager().getNumWorkers());

        if (numWorkers == 0)
            return;

        for (;;) {
            barrierCounter = numWorkers;
            abort = 0;
            Atomic::memory_fence_producer();
            state = 1;

            for (;;) {
                const int local_state( *static_cast<volatile int *>(&state) );
                if (local_state == 0) {
                    const int local_abort(*static_cast<volatile int *>(&abort));
                    if (local_abort == 1)
                        break;
                    if (TaskExecutor<Options>::getTaskQueue().gotWorkSafe())
                        break;
                    return;
                }

                const int local_abort(*static_cast<volatile int *>(&abort));
                if (local_abort == 1 || TaskExecutor<Options>::getTaskQueue().gotWorkSafe()) {
                    while (*static_cast<volatile int *>(&state) != 0)
                        while (TaskExecutor<Options>::executeTasks());
                    break;
                }
            }
        }
    }

    void signalNewWork() {
        const int local_abort(*static_cast<volatile int *>(&abort));
        if (local_abort != 1) {
            abort = 1;
			Atomic::memory_fence_producer();
		}
    }
};

#endif // __BARRIERPROTOCOL_HPP__
