#ifndef __TEST_TASKQUEUEPRIO_HPP_
#define __TEST_TASKQUEUEPRIO_HPP_

#include "test_taskqueue_impl.hpp"
#include "option/taskqueue_prio.hpp"

#include <string>

class TestTaskQueuePrio : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {
        typedef TaskQueuePrioUnsafe<OpDefault> TaskQueueUnsafeType;
    };

    struct LowPrioTask : public TaskBase<OpDefault> {
        std::string name;
        LowPrioTask(std::string name_) : name(name_) {}
        void run() {}
    };

    struct HighPrioTask : public TaskBase<OpDefault> {
        std::string name;
        HighPrioTask(std::string name_) : name(name_) {
            is_prioritized = true;
        }
        void run() {}
    };

    static bool testTaskQueue(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

    static bool testEraseIf(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

    static bool testPrio(std::string &name) { name = "testTaskQueuePrio";
        typename OpDefault::TaskQueueUnsafeType q;

        union {
            TaskBase<OpDefault> *taskBase;
            LowPrioTask *task;
            HighPrioTask *task2;
        };

        if (!q.empty()) return false;
        q.push_back(new LowPrioTask("B"));
        q.push_back(new HighPrioTask("A"));
        q.push_back(new LowPrioTask("B"));

        if (!q.pop_front(taskBase) || task->name != "A") return false;
        if (!q.pop_front(taskBase) || task->name != "B") return false;
        if (!q.pop_front(taskBase) || task->name != "B") return false;
        if (!q.empty() || q.pop_front(taskBase)) return false;

        q.push_back(new LowPrioTask("B"));
        q.push_back(new HighPrioTask("A"));
        q.push_back(new LowPrioTask("B"));

        if (!q.pop_back(taskBase) || task->name != "A") return false;
        if (!q.pop_back(taskBase) || task->name != "B") return false;
        if (!q.pop_back(taskBase) || task->name != "B") return false;
        if (!q.empty() || q.pop_front(taskBase)) return false;

        q.push_front(new LowPrioTask("B"));
        q.push_front(new HighPrioTask("A"));
        q.push_front(new LowPrioTask("B"));

        if (!q.pop_front(taskBase) || task->name != "A") return false;
        if (!q.pop_front(taskBase) || task->name != "B") return false;
        if (!q.pop_front(taskBase) || task->name != "B") return false;
        if (!q.empty() || q.pop_front(taskBase)) return false;

        q.push_front(new LowPrioTask("B"));
        q.push_front(new HighPrioTask("A"));
        q.push_front(new LowPrioTask("B"));

        if (!q.pop_back(taskBase) || task->name != "A") return false;
        if (!q.pop_back(taskBase) || task->name != "B") return false;
        if (!q.pop_back(taskBase) || task->name != "B") return false;
        if (!q.empty() || q.pop_front(taskBase)) return false;

        return true;
    }

public:

    std::string getName() { return "TestTaskQueuePrio"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testTaskQueue, testEraseIf, testPrio
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_TASKQUEUEPRIO_HPP_
