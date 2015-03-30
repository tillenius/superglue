#ifndef SG_TEST_TASKQUEUE_IMPL_HPP_INCLUDED
#define SG_TEST_TASKQUEUE_IMPL_HPP_INCLUDED

template<typename Options>
struct TaskQueueTest {

    struct MyTask : public TaskBase<Options> {
        std::string name;
        MyTask(std::string name_) : name(name_) {}
        void run() {}
    };

    struct IsBPred {
        bool operator()(TaskBase<Options> *task) {
            return ((MyTask *) task)->name == "B";
        }
    };

    static const std::string &name(TaskBase<Options> *task) {
        return ((MyTask *) task)->name;
    }

    static bool testTaskQueueImpl(std::string &testname) { testname = "testTaskQueue";
        typename Options::ReadyListType::unsafe_t q;

        if (!q.empty()) return false;
        q.push_back(new MyTask("A"));
        if (q.empty()) return false;
        q.push_front(new MyTask("B"));
        q.push_back(new MyTask("C"));
        q.push_back(new MyTask("D"));

        TaskBase<Options> *task;

        if (!q.pop_front(task) || name(task) != "B") return false;
        if (!q.pop_front(task) || name(task) != "A") return false;
        if (!q.pop_front(task) || name(task) != "C") return false;
        if (!q.pop_front(task) || name(task) != "D") return false;
        if (!q.empty() || q.pop_front(task)) return false;

        q.push_front(new MyTask("A"));
        q.push_front(new MyTask("B"));
        q.push_front(new MyTask("C"));
        q.push_front(new MyTask("D"));

        if (!q.pop_back(task) || name(task) != "A") return false;
        if (!q.pop_back(task) || name(task) != "B") return false;
        if (!q.pop_back(task) || name(task) != "C") return false;
        if (!q.pop_back(task) || name(task) != "D") return false;
        if (!q.empty() || q.pop_back(task)) return false;

        if (!q.empty()) return false;
        q.push_back(new MyTask("A"));
        if (q.empty()) return false;
        q.push_front(new MyTask("B"));

        if (!q.pop_front(task)) return false;
        if (name(task) != "B") return false;
        if (!q.pop_front(task)) return false;
        if (name(task) != "A") return false;
        if (q.pop_front(task)) return false;

        q.push_front(new MyTask("A"));
        q.push_front(new MyTask("B"));

        if (!q.pop_back(task)) return false;
        if (name(task) != "A") return false;
        if (!q.pop_back(task)) return false;
        if (name(task) != "B") return false;
        if (q.pop_back(task)) return false;

        return true;
    }

    static bool testEraseIfImpl(std::string &testname) { testname = "testEraseIf";
        TaskBase<Options> *task;
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("E"));
            q.push_back(new MyTask("C"));
            q.push_back(new MyTask("D"));
            q.erase_if(IsBPred());
            if (!q.pop_front(task) || name(task) != "A") return false;
            if (!q.pop_front(task) || name(task) != "E") return false;
            if (!q.pop_front(task) || name(task) != "C") return false;
            if (!q.pop_front(task) || name(task) != "D") return false;
            if (!q.empty() || q.pop_front(task)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("C"));
            q.erase_if(IsBPred());
            if (!q.pop_front(task) || name(task) != "A") return false;
            if (!q.pop_front(task) || name(task) != "C") return false;
            if (!q.empty() || q.pop_front(task)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.erase_if(IsBPred());
            if (!q.empty() || q.pop_front(task)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("C"));
            q.push_back(new MyTask("B"));
            q.erase_if(IsBPred());
            if (!q.pop_front(task) || name(task) != "A") return false;
            if (!q.pop_front(task) || name(task) != "C") return false;
            if (!q.empty() || q.pop_front(task)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("C"));
            q.erase_if(IsBPred());
            if (!q.pop_front(task) || name(task) != "A") return false;
            if (!q.pop_front(task) || name(task) != "C") return false;
            if (!q.empty() || q.pop_front(task)) return false;
        }

        return true;
    }
};

#endif // SG_TEST_TASKQUEUE_IMPL_HPP_INCLUDED
