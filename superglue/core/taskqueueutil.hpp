#ifndef __TASKQUEUEUTIL_HPP__
#define __TASKQUEUEUTIL_HPP__

// push_back
// push_front
// push_front_list
// pop_back
// pop_front
// empty
// swap
// size

template<typename TaskQueueUnsafe>
class TaskQueueSafe {
    template<typename, typename> friend struct Log_DumpState;
    template<typename> friend class TaskQueueExclusive;

    TaskQueueUnsafe queue;

private:
    SpinLock spinlock;

    TaskQueueSafe(const TaskQueueSafe &);
    const TaskQueueSafe &operator=(const TaskQueueSafe &);

    SpinLock &getLock() { return spinlock; }
    TaskQueueUnsafe &getUnsafeQueue() { return queue; }

    typedef typename TaskQueueUnsafe::value_type value_type;

public:
    typedef TaskQueueUnsafe unsafe_t;

    TaskQueueSafe() {}

    void push_back(value_type *elem) {
        SpinLockScoped lock(spinlock);
        queue.push_back(elem);
    }

    void push_front(value_type *elem) {
        SpinLockScoped lock(spinlock);
        queue.push_front(elem);
    }

    // takes ownership of input list
    void push_front_list(TaskQueueUnsafe &list) {
        SpinLockScoped lock(spinlock);
        queue.push_front_list(list);
    }

    bool pop_front(value_type * &elem) {
        SpinLockScoped lock(spinlock);
        return queue.pop_front(elem);
    }

    bool pop_back(value_type * &elem) {
        if (queue.empty())
            return false;
        SpinLockScoped lock(spinlock);
        return queue.pop_back(elem);
    }

    bool try_steal(value_type * &elem) {
        SpinLockTryLock lock(spinlock);
        if (!lock.success)
            return false;
        return queue.pop_back(elem);
    }

    void swap(TaskQueueUnsafe &rhs) {
        SpinLockScoped lock(spinlock);
        queue.swap(rhs);
    }

    bool empty_safe() { 
        SpinLockScoped lock(spinlock);
        return queue.empty();
    }
};

template<typename TaskQueueUnsafe>
class TaskQueueExclusive {
//    typedef typename TaskQueueSafe::unsafe_t unsafequeue_t;
    typedef typename TaskQueueUnsafe::value_type value_type;

private:
    SpinLockScoped lock;
    TaskQueueUnsafe &queue;

    TaskQueueExclusive(const TaskQueueExclusive &);
    const TaskQueueExclusive &operator=(const TaskQueueExclusive &);

public:
    TaskQueueExclusive(TaskQueueSafe<TaskQueueUnsafe> &tq) : lock(tq.getLock()), queue(tq.getUnsafeQueue()) {}
    void push_back(value_type *elem) { queue.push_back(elem); }
    void swap(TaskQueueUnsafe &rhs) { queue.swap(rhs); }
    bool empty() { return queue.empty(); }
};

#endif // __TASKQUEUEUTIL_HPP__
