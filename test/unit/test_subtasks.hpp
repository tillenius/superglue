#ifndef __TEST_SUBTASKS_HPP__
#define __TEST_SUBTASKS_HPP__

#include <string>

class TestSubtasks : public TestCase {

    struct Options : public DefaultOptions<Options> {
        typedef Enable PassTaskExecutor;
        typedef Enable Subtasks;
    };

    struct RunOrder {
        size_t order[10];
        size_t index;
        void store(size_t value) {
            size_t pos = Atomic::increase_nv(&index);
            order[pos-1] = value;
        }
    };

    static bool testSubtasks(std::string &name) { name = "testSubtasks";

        static volatile bool subtask_may_finish = false;
        static RunOrder r;

        struct Subtask : public Task<Options> {
            size_t value;
            Subtask(size_t value_) : value(value_) {}
            void run(TaskExecutor<Options> &te) {
                while (!subtask_may_finish);
                r.store(value);
            }
        };

        struct BigTask : public Task<Options> {
            BigTask(Handle<Options> &h) {
                registerAccess(ReadWriteAdd::write, &h);
            }
            void run(TaskExecutor<Options> &te) {
                r.store(0);
                te.submit(new Subtask(1));
                te.submit(new Subtask(2));
                te.submit(new Subtask(3));
                te.submit(new Subtask(4));
            }
        };

        struct NextTask : public Task<Options> {
            NextTask(Handle<Options> &h) {
                registerAccess(ReadWriteAdd::write, &h);
            }
            void run(TaskExecutor<Options> &te) {
                r.store(5);
                subtask_may_finish = true;
            }
        };


        ThreadManager<Options> tm;
        Handle<Options> h;

        tm.submit(new BigTask(h));
        tm.submit(new NextTask(h)); // may start before bigtasks subtasks are finished
        tm.barrier();

        assert(r.order[0] == 0);
        assert(r.order[1] == 5);
        assert(1 <= r.order[2] && r.order[2] <= 4);
        assert(1 <= r.order[3] && r.order[3] <= 4);
        assert(1 <= r.order[4] && r.order[4] <= 4);
        assert(1 <= r.order[5] && r.order[5] <= 4);

        return true;
    }

    static bool testSubtasksDep(std::string &name) { name = "testSubtasksDep";
        static volatile bool subtask_finished = false;
        static RunOrder r;

        struct Subtask : public Task<Options> {
            size_t value;
            Subtask(size_t value_) : value(value_) {}
            void run(TaskExecutor<Options> &te) {
                r.store(value);
                subtask_finished = true;
            }
        };

        struct BigTask : public Task<Options> {
            BigTask(Handle<Options> &h) {
                registerAccess(ReadWriteAdd::write, &h);
            }
            void run(TaskExecutor<Options> &te) {
                r.store(0);
                te.submit(new Subtask(1));
                te.submit(new Subtask(2));
                te.submit(new Subtask(3));
                te.submit(new Subtask(4));
                // subtasks may start directly
                while (!subtask_finished);
            }
        };

        struct NextTask : public Task<Options> {
            NextTask(Handle<Options> &h) {
                registerAccess(ReadWriteAdd::write, &h);
            }
            void run(TaskExecutor<Options> &te) {
                r.store(5);
            }
        };


        ThreadManager<Options> tm;
        Handle<Options> h;

        tm.submit(new BigTask(h));
        tm.submit(new NextTask(h)); // must wait on BigTask
        tm.barrier();

        assert(r.order[0] == 0);
        assert(r.order[5] == 5);
        assert(1 <= r.order[1] && r.order[1] <= 4);
        assert(1 <= r.order[2] && r.order[2] <= 4);
        assert(1 <= r.order[3] && r.order[3] <= 4);
        assert(1 <= r.order[4] && r.order[4] <= 4);

        return true;
    }

public:

    std::string getName() { return "TestSubtasks"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
                testSubtasks, testSubtasksDep
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_SUBTASKS_HPP__
