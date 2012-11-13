#ifndef __STEALORDER_HPP__
#define __STEALORDER_HPP__

#include <cstddef>
#include <iostream>

template<typename Options> class ThreadManager;
template<typename Options> class TaskBase;
template<typename Options> class TaskQueue;

// Start from first queue and search upwards
template<typename Options>
class DefaultStealOrder {
public:
    // returns true if a task was successfully stolen, false otherwise
    static bool steal(ThreadManager<Options> &tm, size_t id, TaskBase<Options> *&dest) {
        TaskQueue<Options> **taskQueues(tm.getTaskQueues());
        const size_t numQueues = tm.getNumQueues();

        for (size_t i = 0; i < numQueues; ++i) {
            if (i == id)
                continue;
            if (taskQueues[i]->steal(dest))
                return true;
        }
        return false;
    }
};

// Start from current queue and search upwards
template<typename Options>
class StealOrderUpwards {
public:
    // returns true if a task was successfully stolen, false otherwise
    static bool steal(ThreadManager<Options> &tm, size_t id, TaskBase<Options> *&dest) {
        TaskQueue<Options> **taskQueues(tm.getTaskQueues());
        const size_t numQueues = tm.getNumQueues();

        for (size_t i = id+1; i < numQueues; ++i)
            if (taskQueues[i]->steal(dest))
                return true;
        for (size_t i = 0; i < id; ++i)
            if (taskQueues[i]->steal(dest))
                return true;
        return false;
    }
};

// Hard coded to steal from same package first on a 64 core machine
template<typename Options>
class StealOrderPackageFirst {
public:
    // returns true if a task was successfully stolen, false otherwise
    static bool steal(ThreadManager<Options> &tm, size_t id, TaskBase<Options> *&dest) {
        TaskQueue<Options> **taskQueues(tm.getTaskQueues());
        const size_t numQueues = tm.getNumQueues();
        size_t mycore = id & 0x7;
        size_t package = id >> 3;

        for (size_t i = 1; i < 8; ++i)
            if (taskQueues[(package<<3)+(i^mycore)]->steal(dest))
                return true;

        for (size_t j = 1; j < 8; ++j)
            for (size_t i = 0; i < 8; ++i)
                if (taskQueues[((j^package)<<3)+(i^mycore)]->steal(dest))
                    return true;
        return false;
    }
};

#endif //  __STEALORDER_HPP__
