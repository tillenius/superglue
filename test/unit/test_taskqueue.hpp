#ifndef __TEST_TASKQUEUE_HPP_
#define __TEST_TASKQUEUE_HPP_

#include <string>

class TestTaskQueue : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {};

    struct Task : public TaskBase<OpDefault> {
        std::string name;
        Task(std::string name_) : name(name_) {}
        void run() {}
    };

    static bool testTaskQueue(std::string &name) { name = "testTaskQueue";
        TaskQueue<OpDefault> q;

        if (q.gotWork()) return false;
        q.push_back(new Task("A"));
        if (!q.gotWork()) return false;
        q.push_front(new Task("B"));

        union {
          TaskBase<OpDefault> *taskBase;
          Task *task;
        };

        if (!q.get(taskBase)) return false;
        if (task->name != "B") return false;
        if (!q.get(taskBase)) return false;
        if (task->name != "A") return false;
        if (q.get(taskBase)) return false;

        q.push_front(new Task("A"));
        q.push_front(new Task("B"));

        if (!q.steal(taskBase)) return false;
        if (task->name != "A") return false;
        if (!q.steal(taskBase)) return false;
        if (task->name != "B") return false;
        if (q.steal(taskBase)) return false;

        return true;
    }

public:

    std::string getName() { return "TestTaskQueue"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testTaskQueue
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_TASKQUEUE_HPP_
