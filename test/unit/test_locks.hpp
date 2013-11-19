#ifndef __TEST_LOCKS_HPP_
#define __TEST_LOCKS_HPP_

#include "core/accesstypes.hpp"

#include <string>


class TestLocks : public TestCase {
    struct OpLockable : public DefaultOptions<OpLockable> {
        typedef ReadWriteAdd AccessInfoType;
        typedef Enable Lockable;
    };

    static const char *getName(OpLockable) { return "testLockable"; }

    template<typename Op>
    class MyTask : public Task<Op, 1> {
    private:
        size_t *value;

    public:
        MyTask(Handle<Op> &h, size_t *value_) : value(value_) {
            this->registerAccess(ReadWriteAdd::add, &h);
        }
        void run() { *value += 1; }
    };

    template<typename Op>
    static bool testLockable(std::string &name) { name = getName(Op());

        ThreadManager<Op> tm;
        Handle<Op> h;

        size_t value = 0;

        for (size_t i = 0; i < 1000; ++i)
            tm.submit(new MyTask<Op>(h, &value));
        tm.barrier();

        return value == 1000;
    }

public:

    std::string getName() { return "TestLocks"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
                testLockable<OpLockable>
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_LOCKS_HPP_

