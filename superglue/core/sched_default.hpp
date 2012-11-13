#ifndef __SCHED_DEFAULT_HPP__
#define __SCHED_DEFAULT_HPP__

#include "core/types.hpp"
template<typename Options> class TaskBase;

template<typename Options, typename T = typename Options::TaskStealableFlag> struct DefaultScheduler_StealableFlag;

template<typename Options>
struct DefaultScheduler_StealableFlag<Options, typename Options::Disable> {
    static bool stealTask(typename Types<Options>::taskdeque_t &taskQueue, TaskBase<Options> * &elem) {
        elem = taskQueue.back();
        taskQueue.pop_back();
        return true;
    }
};

template<typename Options>
struct DefaultScheduler_StealableFlag<Options, typename Options::Enable> {
    static bool stealTask(typename Types<Options>::taskdeque_t &taskQueue, TaskBase<Options> * &elem) {
        for (size_t i = taskQueue.size(); i != 0; --i)
            if (taskQueue[i-1]->isStealable()) {
              elem = taskQueue[i-1];
              taskQueue.erase(taskQueue.begin() + i-1);
              return true;
            }
        return false;
    }
};

template<typename Options>
class DefaultScheduler
  : public DefaultScheduler_StealableFlag<Options> {
public:
    static void getTask(typename Types<Options>::taskdeque_t &taskQueue, TaskBase<Options> * &elem) {
        elem = taskQueue.front();
        taskQueue.pop_front();
    }

    // initial placement of task
    //   cpuid -- placement supplied by user (or -1)
    // returns which thread should execute the task
    static size_t place(TaskBase<Options> *task, int cpuid, size_t numThreads) {
        if (cpuid == -1)
            return 0;
        return cpuid % numThreads;
    }
};

#endif // __SCHED_DEFAULT_HPP__

