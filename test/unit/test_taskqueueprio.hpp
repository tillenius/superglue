#ifndef SG_TEST_TASKQUEUEPRIO_HPP_INCLUDED
#define SG_TEST_TASKQUEUEPRIO_HPP_INCLUDED

#include "test_taskqueue_impl.hpp"
#include "sg/option/taskqueue_prio.hpp"

#include <string>

using namespace sg;

class TestTaskQueuePrio : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {
        typedef TaskQueuePrio<OpDefault> ReadyListType;
        typedef TaskQueuePrio<OpDefault> WaitListType;
    };

    struct NamedTask : public Task<OpDefault> {
        std::string name;
        NamedTask(const std::string &name_) : name(name_) {}
    };

    struct LowPrioTask : public NamedTask {
        LowPrioTask(const std::string &name_) : NamedTask(name_) {}
        void run() {}
    };

    struct HighPrioTask : public NamedTask {
        HighPrioTask(const std::string &name_) : NamedTask(name_) {
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

    static const std::string &name(TaskBase<OpDefault> *task) {
        return static_cast<NamedTask *>(task)->name;
    }

    static bool testPrio(std::string &testname) { testname = "testTaskQueuePrio";
        typedef OpDefault::ReadyListType::unsafe_t TaskQueueUnsafe;

        TaskQueueUnsafe q;
	TaskBase<OpDefault> *task;

        if (!q.empty()) return false;
        q.push_back(new LowPrioTask("B"));
        q.push_back(new HighPrioTask("A"));
        q.push_back(new LowPrioTask("B"));

        if (!q.pop_front(task) || name(task) != "A") return false;
        if (!q.pop_front(task) || name(task) != "B") return false;
        if (!q.pop_front(task) || name(task) != "B") return false;
        if (!q.empty() || q.pop_front(task)) return false;

        q.push_back(new LowPrioTask("B"));
        q.push_back(new HighPrioTask("A"));
        q.push_back(new LowPrioTask("B"));

        if (!q.pop_back(task) || name(task) != "A") return false;
        if (!q.pop_back(task) || name(task) != "B") return false;
        if (!q.pop_back(task) || name(task) != "B") return false;
        if (!q.empty() || q.pop_front(task)) return false;

        q.push_front(new LowPrioTask("B"));
        q.push_front(new HighPrioTask("A"));
        q.push_front(new LowPrioTask("B"));

        if (!q.pop_front(task) || name(task) != "A") return false;
        if (!q.pop_front(task) || name(task) != "B") return false;
        if (!q.pop_front(task) || name(task) != "B") return false;
        if (!q.empty() || q.pop_front(task)) return false;

        q.push_front(new LowPrioTask("B"));
        q.push_front(new HighPrioTask("A"));
        q.push_front(new LowPrioTask("B"));

        if (!q.pop_back(task) || name(task) != "A") return false;
        if (!q.pop_back(task) || name(task) != "B") return false;
        if (!q.pop_back(task) || name(task) != "B") return false;
        if (!q.empty() || q.pop_front(task)) return false;

        return true;
    }

public:

    std::string get_name() { return "TestTaskQueuePrio"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testTaskQueue, testEraseIf, testPrio
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_TASKQUEUEPRIO_HPP_INCLUDED
