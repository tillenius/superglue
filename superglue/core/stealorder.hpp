#ifndef __STEALORDER_HPP__
#define __STEALORDER_HPP__

#include <cstddef>
#include <iostream>

template<typename Options> class ThreadManager;
template<typename Options> class TaskBase;
template<typename Options> class TaskQueue;

// Start from random queue and search upwards
template<typename Options>
class DefaultStealOrder {
private:
    size_t seed;

public:

    void init(TaskExecutor<Options> *parent) {
        seed = parent->id;
    }

    bool steal(ThreadManager<Options> &tm, size_t id, TaskBase<Options> *&dest) {
        TaskQueue<Options> **taskQueues(tm.getTaskQueues());
        const size_t numQueues = tm.getNumQueues();
        seed = seed * 1664525 + 1013904223;
        const size_t mask((numQueues-1));
        const size_t random(seed & mask);

        for (size_t i = random; i < numQueues; ++i) {
            if (i == id)
                continue;
            if (taskQueues[i]->steal(dest))
                return true;
        }
        for (size_t i = 0; i < random; ++i) {
            if (i == id)
                continue;
            if (taskQueues[i]->steal(dest))
                return true;
        }
        return false;
    }
};

#endif //  __STEALORDER_HPP__
