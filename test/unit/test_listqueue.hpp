#ifndef __TEST_LISTQUEUE_HPP_
#define __TEST_LISTQUEUE_HPP_

#include <string>

class TestListQueue : public TestCase {
    struct OpPaused : public DefaultOptions<OpPaused> {
        typedef Enable PauseExecution;
        typedef Enable ListQueue;
        typedef ReadWrite AccessInfoType;
        typedef Disable Locking;
    };

    class DepTask : public Task<OpPaused, 1> {
    private:
        size_t *value;
        bool *success;
        size_t idx;
    public:
        DepTask(Handle<OpPaused> &h, size_t *value_, bool *success_, size_t idx_)
         : value(value_), success(success_), idx(idx_) {
            registerAccess(ReadWrite::write, &h);
        }
        void run() {
            if (*value != idx)
                *success = false;
            ++(*value);
        }
    };

    static bool testDependent(std::string &name) { name = "testDependent";
        {
            ThreadManager<OpPaused> tm;
            Handle<OpPaused> h;
            size_t value = 0;
            bool success = true;
            for (size_t i = 0; i < 1000; ++i)
                tm.submit(new DepTask(h, &value, &success, i));
            if (value != 0)
                return false;
            tm.setMayExecute(true);
            tm.barrier();

            if (value != 1000)
                return false;

            if (!success)
                return false;
        }

        {
            ThreadManager<OpPaused> tm;
            Handle<OpPaused> h2[1000];
            size_t value = 0;
            bool success = true;
            for (size_t i = 0; i < 1000; ++i)
                tm.submit(new DepTask(h2[i], &value, &success, i));
            if (value != 0) {
                tm.setMayExecute(true);
                return false;
            }

            tm.setMayExecute(true);
            tm.barrier();
            if (value == 0)
                return false;
            return !success;
        }
    }

public:

    std::string getName() { return "TestListQueue"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testDependent
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_LISTQUEUE_HPP_
