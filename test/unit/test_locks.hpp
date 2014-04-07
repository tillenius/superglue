#ifndef SG_TEST_LOCKS_HPP_INCLUDED
#define SG_TEST_LOCKS_HPP_INCLUDED

#include <string>

class TestLocks : public TestCase {
    struct OpLockable : public DefaultOptions<OpLockable> {};

    static const char *get_name(OpLockable) { return "testLockable"; }

    template<typename Op>
    class MyTask : public Task<Op, 1> {
    private:
        size_t *value;

    public:
        MyTask(Handle<Op> &h, size_t *value_) : value(value_) {
            this->register_access(ReadWriteAdd::add, h);
        }
        void run() { *value += 1; }
    };

    template<typename Op>
    static bool testLockable(std::string &name) { name = get_name(Op());

        SuperGlue<Op> sg;
        Handle<Op> h;

        size_t value = 0;

        for (size_t i = 0; i < 1000; ++i)
            sg.submit(new MyTask<Op>(h, &value));
        sg.barrier();

        return value == 1000;
    }

public:

    std::string get_name() { return "TestLocks"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
                testLockable<OpLockable>
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_LOCKS_HPP_INCLUDED

