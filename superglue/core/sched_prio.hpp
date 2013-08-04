#ifndef __SCHED_PRIO_HPP__
#define __SCHED_PRIO_HPP__

#include <cstddef>

template<typename Options, int N> class Task;

template<typename Options>
class PrioScheduler {
public:
    typedef typename Options::TaskPriorities requires_TaskPriorities;
    typedef typename Types<Options>::taskdeque_t taskdeque_t;

public:
    static void getTask(taskdeque_t &taskQueue, TaskBase<Options> * &elem) {
        // Uses linear search through whole ready queue. Possible performance issue.
        size_t pos = 0;
        int maxprio = 0;
        const size_t size = taskQueue.size();
        for (size_t i = 0; i < size; ++i) {
            if (taskQueue[i]->getPriority() > maxprio) {
                maxprio = taskQueue[i]->getPriority();
                pos = i;
            }
        }

        elem = taskQueue[pos];
        // could swap with last element and erase last, which is quicker but destroys order
        //taskQueue[pos] = taskQueue[size-1];
        //taskQueue.erase(taskQueue.begin() + (size-1));
        // instead, just remove it and move all other elements:
        taskQueue.erase(taskQueue.begin() + pos);
    }

    static bool stealTask(taskdeque_t &taskQueue, TaskBase<Options> * &elem) {
        getTask(taskQueue, elem);
        return true;
    }
};

#endif // __SCHED_PRIO_HPP__

