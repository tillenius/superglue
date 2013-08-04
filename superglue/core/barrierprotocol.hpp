#ifndef __BARRIERPROTOCOL_HPP__
#define __BARRIERPROTOCOL_HPP__

#include "platform/atomic.hpp"
#include "platform/mutex.hpp"
#include "core/log.hpp"

template <typename Options> class ThreadManager;
template <typename Options> class WorkerThread;

template <typename Options>
class BarrierProtocol
  : public TaskExecutor<Options>
{
private:
    size_t barrierCounter;
    size_t state;
    size_t abort;

private:

    void waitAfterBarrier(WorkerThread<Options> &wwt) {
        // if we have new work in our queue, then abort
        if (wwt.getTaskQueue().gotWork()) {
            abort = 1;
            Atomic::memory_fence_producer(); // propagate abort before updating state
            const size_t count = Atomic::decrease_nv(&barrierCounter);
            if (count == 0)
                state = 0;
            return;
        }

        // leave barrier
        const size_t seenstate = state;
        const size_t count = Atomic::decrease_nv(&barrierCounter);
        if (count == 0) {
            state = 0;
            return;
        }

        // wait for everyone to leave barrier
        for (;;) {
            if (seenstate != state)
                return;
            if (abort == 1)
                return;
            Atomic::rep_nop(); // this includes a compiler barrier to force 'state' and 'abort' to be reread
        }
    }

public:
    BarrierProtocol(ThreadManager<Options> &tm_)
      : TaskExecutor<Options>(0, tm_),
        barrierCounter(0), state(0)
    {
    }

    void waitForWork() {
        Atomic::rep_nop();
    }

    // Leave old barrier
    // returns true if we successfully left old barrier
    bool leaveOldBarrier(WorkerThread<Options> &wwt) {
        // currently aborting state 1

        if (state != 2) // continue waiting?
            return false;

        waitAfterBarrier(wwt);
        return true;
    }

    // Called from WorkerThread: Wait at barrier if requested.
    // returns true if we must wait to leave the barrier
    bool waitAtBarrier(WorkerThread<Options> &wwt) {
        // here we can be in state 2 if we have aborted state 2 early
        if (state != 1)
            return false;

        const size_t numWorkers(TaskExecutor<Options>::getThreadManager().getNumWorkers());
        const size_t count = Atomic::increase_nv(&barrierCounter);

        if (count == numWorkers) {
            // last to enter the barrier, go to next state
            state = 2;
            Atomic::memory_fence_producer();
            waitAfterBarrier(wwt);
            return false;
        }

        TaskQueue<Options> &taskQueue(wwt.getTaskQueue());
        // wait at barrier
        for (;;) {
            if (state == 2) {
                waitAfterBarrier(wwt);
                return false;
            }
            if (taskQueue.gotWork()) {
                abort = 1;
                Atomic::memory_fence_producer();
                return true;
            }
            if (abort == 1) {
                // go back to work, but remember that we need to leave the barrier
                // when it reaches state 2
                return true;
            }
            Atomic::rep_nop(); // this includes a compiler barrier to force 'state' and 'abort' to be reread
        }
        return false;
    }

    // cannot be invoked by more than one thread at a time
    void barrier() {
        while (TaskExecutor<Options>::executeTasks());

        if (TaskExecutor<Options>::getThreadManager().getNumWorkers() == 0)
            return;

        do {
            abort = 0;
            state = 1;
            Atomic::memory_fence_producer();
            while (*static_cast<volatile size_t *>(&state) != 0)
                while (TaskExecutor<Options>::executeTasks());
            Atomic::memory_fence_consumer(); // to make sure we don't read abort & state in wrong order
        } while (abort == 1 || TaskExecutor<Options>::getTaskQueue().gotWork());
    }

    void signalNewWork() {
        abort = 1;
        Atomic::memory_fence_producer();
    }
};

#endif // __BARRIERPROTOCOL_HPP__
