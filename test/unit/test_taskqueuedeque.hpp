#ifndef __TEST_DEQUETASKQUEUE_HPP_
#define __TEST_DEQUETASKQUEUE_HPP_

#include "option/taskqueue_deque.hpp"
#include "test_taskqueue_impl.hpp"

#include <string>

class TestTaskQueueDeque : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {
        typedef TaskQueueDequeUnsafe<OpDefault> TaskQueueUnsafeType;
    };

    static bool testTaskQueue(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

    static bool testEraseIf(std::string &name) {
        return TaskQueueTest<OpDefault>::testTaskQueueImpl(name);
    }

public:

    std::string getName() { return "TestTaskQueueDeque"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testTaskQueue, testEraseIf
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_DEQUETASKQUEUE_HPP_
