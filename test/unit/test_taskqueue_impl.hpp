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

    static bool testTaskQueueImpl(std::string &name) { name = "testTaskQueue";
        typename Options::ReadyListType::unsafe_t q;

        if (!q.empty()) return false;
        q.push_back(new MyTask("A"));
        if (q.empty()) return false;
        q.push_front(new MyTask("B"));
        q.push_back(new MyTask("C"));
        q.push_back(new MyTask("D"));

        union {
            TaskBase<Options> *taskBase;
            MyTask *task;
        };

        if (!q.pop_front(taskBase) || task->name != "B") return false;
        if (!q.pop_front(taskBase) || task->name != "A") return false;
        if (!q.pop_front(taskBase) || task->name != "C") return false;
        if (!q.pop_front(taskBase) || task->name != "D") return false;
        if (!q.empty() || q.pop_front(taskBase)) return false;

        q.push_front(new MyTask("A"));
        q.push_front(new MyTask("B"));
        q.push_front(new MyTask("C"));
        q.push_front(new MyTask("D"));

        if (!q.pop_back(taskBase) || task->name != "A") return false;
        if (!q.pop_back(taskBase) || task->name != "B") return false;
        if (!q.pop_back(taskBase) || task->name != "C") return false;
        if (!q.pop_back(taskBase) || task->name != "D") return false;
        if (!q.empty() || q.pop_back(taskBase)) return false;

        if (!q.empty()) return false;
        q.push_back(new MyTask("A"));
        if (q.empty()) return false;
        q.push_front(new MyTask("B"));

        if (!q.pop_front(taskBase)) return false;
        if (task->name != "B") return false;
        if (!q.pop_front(taskBase)) return false;
        if (task->name != "A") return false;
        if (q.pop_front(taskBase)) return false;

        q.push_front(new MyTask("A"));
        q.push_front(new MyTask("B"));

        if (!q.pop_back(taskBase)) return false;
        if (task->name != "A") return false;
        if (!q.pop_back(taskBase)) return false;
        if (task->name != "B") return false;
        if (q.pop_back(taskBase)) return false;

        return true;
    }

    static bool testEraseIfImpl(std::string &name) { name = "testEraseIf";
        union {
          TaskBase<Options> *taskBase;
          MyTask *task;
        };
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("E"));
            q.push_back(new MyTask("C"));
            q.push_back(new MyTask("D"));
            q.erase_if(IsBPred());
            if (!q.pop_front(taskBase) || task->name != "A") return false;
            if (!q.pop_front(taskBase) || task->name != "E") return false;
            if (!q.pop_front(taskBase) || task->name != "C") return false;
            if (!q.pop_front(taskBase) || task->name != "D") return false;
            if (!q.empty() || q.pop_front(taskBase)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("C"));
            q.erase_if(IsBPred());
            if (!q.pop_front(taskBase) || task->name != "A") return false;
            if (!q.pop_front(taskBase) || task->name != "C") return false;
            if (!q.empty() || q.pop_front(taskBase)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.erase_if(IsBPred());
            if (!q.empty() || q.pop_front(taskBase)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("C"));
            q.push_back(new MyTask("B"));
            q.erase_if(IsBPred());
            if (!q.pop_front(taskBase) || task->name != "A") return false;
            if (!q.pop_front(taskBase) || task->name != "C") return false;
            if (!q.empty() || q.pop_front(taskBase)) return false;
        }
        {
            typename Options::TaskQueueUnsafeType q;
            q.push_back(new MyTask("A"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("B"));
            q.push_back(new MyTask("C"));
            q.erase_if(IsBPred());
            if (!q.pop_front(taskBase) || task->name != "A") return false;
            if (!q.pop_front(taskBase) || task->name != "C") return false;
            if (!q.empty() || q.pop_front(taskBase)) return false;
        }

        return true;
    }
};

#endif // SG_TEST_TASKQUEUE_IMPL_HPP_INCLUDED
