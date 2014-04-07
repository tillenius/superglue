#ifndef SG_TEST_SUBTASKS_HPP_INCLUDED
#define SG_TEST_SUBTASKS_HPP_INCLUDED

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
                register_access(ReadWriteAdd::write, h);
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
                register_access(ReadWriteAdd::write, h);
            }
            void run(TaskExecutor<Options> &te) {
                r.store(5);
                subtask_may_finish = true;
            }
        };


        SuperGlue<Options> sg;
        Handle<Options> h;

        sg.submit(new BigTask(h));
        sg.submit(new NextTask(h)); // may start before bigtasks subtasks are finished
        sg.barrier();

        assert(r.order[0] == 0);
        assert(r.order[1] == 5);
        assert(1 <= r.order[2] && r.order[2] <= 4);
        assert(1 <= r.order[3] && r.order[3] <= 4);
        assert(1 <= r.order[4] && r.order[4] <= 4);
        assert(1 <= r.order[5] && r.order[5] <= 4);

        return true;
    }

    static bool testSubtasksDep(std::string &name) { name = "testSubtasksDep";
        static volatile unsigned int subtask_finished = 0;
        static RunOrder r;

        struct Subtask : public Task<Options> {
            size_t value;
            Subtask(size_t value_) : value(value_) {}
            void run(TaskExecutor<Options> &te) {
                r.store(value);
                Atomic::increase(&subtask_finished);
            }
        };

        struct BigTask : public Task<Options> {
            BigTask(Handle<Options> &h) {
                register_access(ReadWriteAdd::write, h);
            }
            void run(TaskExecutor<Options> &te) {
                r.store(0);
                te.submit(new Subtask(1));
                te.submit(new Subtask(2));
                te.submit(new Subtask(3));
                te.submit(new Subtask(4));
                // subtasks may start directly
                while (subtask_finished != 4);
            }
        };

        struct NextTask : public Task<Options> {
            NextTask(Handle<Options> &h) {
                register_access(ReadWriteAdd::write, h);
            }
            void run(TaskExecutor<Options> &te) {
                r.store(5);
            }
        };


        SuperGlue<Options> sg;
        Handle<Options> h;

        sg.submit(new BigTask(h));
        sg.submit(new NextTask(h)); // must wait on BigTask
        sg.barrier();

        assert(r.order[0] == 0);
        assert(r.order[5] == 5);
        assert(1 <= r.order[1] && r.order[1] <= 4);
        assert(1 <= r.order[2] && r.order[2] <= 4);
        assert(1 <= r.order[3] && r.order[3] <= 4);
        assert(1 <= r.order[4] && r.order[4] <= 4);

        return true;
    }

public:

    std::string get_name() { return "TestSubtasks"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
                testSubtasks, testSubtasksDep
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_SUBTASKS_HPP_INCLUDED
