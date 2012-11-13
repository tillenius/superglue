#ifndef __TEST_TASKS_HPP_
#define __TEST_TASKS_HPP_

#include <string>

class TestTasks : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {};
    struct OpNoSteal : public DefaultOptions<OpNoSteal> {
        typedef Disable Stealing;
    };
    struct OpPaused : public DefaultOptions<OpPaused> {
        typedef Disable Lockable;
        typedef ReadWrite AccessInfoType;
        typedef Enable PauseExecution;
    };

    class MyTask : public Task<OpDefault, 0> {
    private:
        size_t *value;

    public:
        MyTask(size_t *value_) : value(value_) {}
        void run() {
            Atomic::increase(value);
        }
    };

    class ParallelTask : public Task<OpDefault, 0> {
    private:
        size_t *value;
        size_t myValue, otherValue;

    public:
        ParallelTask(size_t *value_, size_t myValue_, size_t otherValue_)
         : value(value_), myValue(myValue_), otherValue(otherValue_) {}
        void run() {
            for (size_t i = 0; i < 1000; ++i)
                Atomic::cas(value, myValue, otherValue);
        }
    };

    static bool testBarrier(std::string &name) { name = "testBarrier";

        ThreadManager<OpDefault> tm;

        size_t value = 0;

        tm.barrier();
        for (size_t i = 0; i < 1000; ++i)
            tm.submit(new MyTask(&value));
        tm.barrier();
        tm.barrier();

        return value == 1000;
    }

    static bool testStealing(std::string &name) { name = "testStealing";
        ThreadManager<OpDefault> tm;
        size_t value = 0;
        for (size_t i = 0; i < 100; ++i) {
            tm.submit(new ParallelTask(&value, 0, 1), 0);
            tm.submit(new ParallelTask(&value, 1, 0), 0);
        }
        tm.barrier();
        return true;
    }

    class NoStealingTask : public Task<OpNoSteal, 0> {
    private:
        ThreadIDType *threadid;
        bool *success;

    public:
        NoStealingTask(ThreadIDType *threadid_, bool *success_)
         : threadid(threadid_), success(success_) {}
        void run() {
            if (*threadid == 0)
                *threadid = ThreadUtil::getCurrentThreadId();
            else if (*threadid != ThreadUtil::getCurrentThreadId())
                *success = false;
        }
    };

    static bool testNoStealing(std::string &name) { name = "testNoStealing";
        ThreadManager<OpNoSteal> tm;
        ThreadIDType threadid = 0;
        bool success = true;
        for (size_t i = 0; i < 100; ++i) {
            tm.submit(new NoStealingTask(&threadid, &success), 0);
            tm.submit(new NoStealingTask(&threadid, &success), 0);
        }
        tm.barrier();
        return success;
    }

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
                tm.setMayExecute(true); // must be enabled when tm is destructed
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

    std::string getName() { return "TestTasks"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
                testBarrier, testStealing, testNoStealing, testDependent
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_TASKS_HPP_
