#ifndef __TEST_RWC_HPP__
#define __TEST_RWC_HPP__

class TestRWC : public TestCase {
    struct Options : public DefaultOptions<Options> {
        typedef ReadWriteConcurrent AccessInfoType;
    };

    class MyTask : public Task<Options, 1> {
    private:
        size_t *flag;
        size_t *value;
        size_t index;

    public:
        MyTask(size_t *flag_, size_t *value_, Handle<Options> &h, size_t i)
        : flag(flag_), value(value_), index(i) {
            registerAccess(ReadWriteConcurrent::concurrent, &h);
        }
        void run() {
            Atomic::decrease(flag);
            while (*flag != 0)
                Atomic::compiler_fence();
            for (size_t i = 0; i < 100000; ++i)
              ++value[index];
        }
    };

    static bool testConcurrent(std::string &name) { name = "testConcurrent";
        ThreadManager<Options> tm;
        size_t num = tm.getNumQueues();
        size_t flag = num;
        std::vector<size_t> values(num);
        Handle<Options> h;
        for (size_t i = 0; i < num; ++i)
            tm.submit(new MyTask(&flag, &values[0], h, i));
        tm.barrier();
        for (size_t i = 0; i < num; ++i)
            if (values[i] != 100000)
                return false;
        return true;
    }

public:

    std::string getName() { return "TestRWC"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
                testConcurrent
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif /* __TEST_RWC_HPP__ */
