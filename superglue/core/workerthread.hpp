#ifndef __WORKERTHREAD_HPP__
#define __WORKERTHREAD_HPP__

#include "platform/threads.hpp"
#include "core/barrierprotocol.hpp"

template<typename Options> class TaskBase;

// ============================================================================
// WorkerThread
// ============================================================================
template<typename Options>
class WorkerThread
  : public TaskExecutor<Options>
{
    template<typename, typename> friend class WorkerThread_PassThreadId;
protected:
    Thread *thread;
    bool terminateFlag;

    WorkerThread(const WorkerThread &);
    const WorkerThread &operator=(const WorkerThread &);

    void waitForWork() {
        Atomic::rep_nop();
    }

    // Called from this thread only
    void mainLoop() {

        for (;;) {

            while (TaskExecutor<Options>::executeTasks());

            TaskExecutor<Options>::tm.barrierProtocol.waitAtBarrier(*this);

            if (*static_cast<volatile bool *>(&terminateFlag)) {
                ThreadUtil::exit();
                return;
            }

            waitForWork();
        }
    }

public:
    int my_barrier_state;

    // Called from this thread only
    WorkerThread(int id_, ThreadManager<Options> &tm_)
      : TaskExecutor<Options>(id_, tm_), terminateFlag(false), my_barrier_state(0)
    {}

    ~WorkerThread() {}

    // Called from other threads
    void join() { thread->join(); }

    // For ThreadManager::getCurrentThread and logging
    Thread *getThread() { return thread; }

    // Called from other threads
    void setTerminateFlag() {
        terminateFlag = true;
    }

    void run(Thread *thread_) {
        this->thread = thread_;
        srand(TaskExecutor<Options>::getId());

        TaskExecutor<Options>::getThreadManager().waitToStartThread();
        mainLoop();
    }
};

#endif // __WORKERTHREAD_HPP__
