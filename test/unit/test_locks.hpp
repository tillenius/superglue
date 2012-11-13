#ifndef __TEST_LOCKS_HPP_
#define __TEST_LOCKS_HPP_

#include "core/accesstypes.hpp"

#include <string>


class TestLocks : public TestCase {
    struct OpLockable : public DefaultOptions<OpLockable> {
        typedef ReadWriteAdd AccessInfoType;
        typedef Enable Lockable;
        typedef Enable ListQueue;
    };

    struct OpLockableVec : public DefaultOptions<OpLockableVec> {
        typedef ReadWriteAdd AccessInfoType;
        typedef Enable Lockable;
        typedef Disable ListQueue;
    };

    static const char *getName(OpLockable) { return "testLockable"; }
    static const char *getName(OpLockableVec) { return "testLockableVec"; }

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
                testLockable<OpLockable>,
                testLockable<OpLockableVec>,
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_LOCKS_HPP_

