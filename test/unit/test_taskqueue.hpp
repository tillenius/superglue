#ifndef SG_TEST_TASKQUEUE_HPP_INCLUDED
#define SG_TEST_TASKQUEUE_HPP_INCLUDED

#include "test_taskqueue_impl.hpp"

#include <string>

class TestTaskQueue : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {};

    static bool testTaskQueue(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

    static bool testEraseIf(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

public:

    std::string get_name() { return "TestTaskQueue"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testTaskQueue, testEraseIf
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_TASKQUEUE_HPP_INCLUDED
