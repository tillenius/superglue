#ifndef __WORKERTHREAD_HPP__
#define __WORKERTHREAD_HPP__

#include "platform/threads.hpp"
#include "platform/mutex.hpp"
#include "core/barrierprotocol.hpp"

template<typename Options> class TaskBase;

// ============================================================================
// WorkerThread
// ============================================================================
template<typename Options>
class WorkerThread
  : public TaskExecutor<Options>
{
    template<typename, typename> friend struct WorkerThread_PassThreadId;
protected:
    Thread *thread;
    bool terminateFlag;
    BarrierProtocol<Options> &barrierProtocol;

    WorkerThread(const WorkerThread &);
    const WorkerThread &operator=(const WorkerThread &);

    // Called from this thread only
    void mainLoop() {

        for (;;) {
            while (TaskExecutor<Options>::executeTasks());

            if (barrierProtocol.waitAtBarrier(*this)) {
                // waiting to leave old aborted barrier
                for (;;) {
                    if (barrierProtocol.leaveOldBarrier(*this)) {
                        // successfully left old barrier
                        break;
                    }
                    TaskExecutor<Options>::executeTasks();
                }
                continue;
            }
            else if (*static_cast<volatile bool *>(&terminateFlag)) {
                ThreadUtil::exit();
                return;
            }
            else {
                // wait for work
                barrierProtocol.waitForWork(); // must include memory barrier to reread 'terminateFlag'
            }
        }
    }

public:
    // Called from this thread only
    WorkerThread(int id_, ThreadManager<Options> &tm_)
      : TaskExecutor<Options>(id_, tm_), terminateFlag(false), barrierProtocol(tm_.barrierProtocol)
    {}

    ~WorkerThread() {}

    // Called from other threads
    void join() { thread->join(); }

    // For ThreadManager::getCurrentThread and logging
    Thread *getThread() { return thread; }

    // Called from other threads
    void setTerminateFlag() {
        terminateFlag = true;
        // no memory fence here, must be followed by a global wake signal which will take care of the fence
    }

    void run(Thread *thread_) {
        this->thread = thread_;
        Atomic::memory_fence_producer();
        srand(TaskExecutor<Options>::getId());

        ThreadManager<Options> &tm(TaskExecutor<Options>::getThreadManager());
        tm.waitToStartThread();
        mainLoop();
    }
};

#endif // __WORKERTHREAD_HPP__
