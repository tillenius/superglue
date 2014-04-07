#ifndef SG_TEST_LISTQUEUE_HPP_INCLUDED
#define SG_TEST_LISTQUEUE_HPP_INCLUDED

#include <string>

class TestListQueue : public TestCase {
    struct OpPaused : public DefaultOptions<OpPaused> {
        typedef Enable PauseExecution;
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
            register_access(ReadWrite::write, h);
        }
        void run() {
            if (*value != idx)
                *success = false;
            ++(*value);
        }
    };

    static bool testDependent(std::string &name) { name = "testDependent";
        {
            SuperGlue<OpPaused> sg;
            Handle<OpPaused> h;
            size_t value = 0;
            bool success = true;
            for (size_t i = 0; i < 1000; ++i)
                sg.submit(new DepTask(h, &value, &success, i));
            if (value != 0)
                return false;
            sg.start_executing();
            sg.barrier();

            if (value != 1000)
                return false;

            if (!success)
                return false;
        }

        {
            SuperGlue<OpPaused> sg;
            Handle<OpPaused> h2[1000];
            size_t value = 0;
            bool success = true;
            for (size_t i = 0; i < 1000; ++i)
                sg.submit(new DepTask(h2[i], &value, &success, i));
            if (value != 0) {
                sg.start_executing();
                return false;
            }

            sg.start_executing();
            sg.barrier();
            if (value == 0)
                return false;
            return !success;
        }
    }

public:

    std::string get_name() { return "TestListQueue"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testDependent
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_LISTQUEUE_HPP_INCLUDED
