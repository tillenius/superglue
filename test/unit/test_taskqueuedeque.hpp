#ifndef SG_TEST_DEQUETASKQUEUE_HPP_INCLUDED
#define SG_TEST_DEQUETASKQUEUE_HPP_INCLUDED

#include "sg/option/taskqueue_deque.hpp"
#include "test_taskqueue_impl.hpp"

#include <string>

class TestTaskQueueDeque : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {
        typedef TaskQueueDeque<OpDefault> TaskQueueType;
    };

    static bool testTaskQueue(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

    static bool testEraseIf(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

public:

    std::string get_name() { return "TestTaskQueueDeque"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testTaskQueue, testEraseIf
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_DEQUETASKQUEUE_HPP_INCLUDED
