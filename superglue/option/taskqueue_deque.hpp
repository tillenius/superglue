#ifndef __TASKQUEUE_DEQUE_HPP__
#define __TASKQUEUE_DEQUE_HPP__

template<typename Options>
class TaskQueueDequeUnsafe {
    typedef typename Types<Options>::taskdeque_t taskdeque_t;
    taskdeque_t q;

public:
    struct ElementData {};
    typedef TaskBase<Options> value_type;

    bool pop_front(TaskBase<Options> * &elem) {
        if (q.empty())
            return false;

        elem = q.front();
        q.pop_front();
        return true;
    }

    bool pop_back(TaskBase<Options> * &elem) {
        if (q.empty())
            return false;
        elem = q.back();
        q.pop_back();
        return true;
    }

    void push_front(TaskBase<Options> *elem) {
        q.push_front(elem);
    }

    void push_back(TaskBase<Options> *elem) {
        q.push_back(elem);
    }

    void push_front_list(TaskQueueDequeUnsafe &rhs) {
        q.insert(q.begin(), rhs.q.begin(), rhs.q.end());
    }

    bool empty() {
        return q.empty();
    }

    template<typename UnaryPredicate>
    void erase_if(UnaryPredicate pred) {
        q.erase(remove_if(q.begin(), q.end(), pred), q.end());
    }

    void swap(TaskQueueDequeUnsafe &rhs) {
        std::swap(q, rhs.q);
    }
};

#endif // __TASKQUEUE_DEQUE_HPP__
