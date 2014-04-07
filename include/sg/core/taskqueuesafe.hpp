#ifndef SG_TASKQUEUESAFE_HPP_INCLUDED
#define SG_TASKQUEUESAFE_HPP_INCLUDED

#include "sg/core/spinlock.hpp"

#include <cassert>

// push_back
// push_front
// push_front_list
// pop_back
// pop_front
// empty
// empty_safe
// swap
//
// private:
//   lock, unlock, get_unsafe_queue

namespace sg {

namespace detail {

class QueueSpinLocked {
    SpinLock spinlock;

public:
    struct ScopedLockHolder : public SpinLockScoped {
        ScopedLockHolder(QueueSpinLocked &qsl) : SpinLockScoped(qsl.spinlock) {}
    };
    struct ScopedLockHolderTry : public SpinLockTryLock {
        ScopedLockHolderTry(QueueSpinLocked &qsl) : SpinLockTryLock(qsl.spinlock) {}
    };
    void lock() { spinlock.lock(); }
    void unlock() { spinlock.unlock(); }
};

template<typename TaskQueueUnsafe, typename LockType>
class TaskQueueSafe {
    template<typename, typename> friend class Log_DumpState;
    template<typename> friend class TaskQueueExclusive;
    typedef typename LockType::ScopedLockHolder ScopedLockHolder;
    typedef typename LockType::ScopedLockHolderTry ScopedLockHolderTry;

private:
    TaskQueueUnsafe queue;
    LockType queuelock;

    TaskQueueSafe(const TaskQueueSafe &);
    const TaskQueueSafe &operator=(const TaskQueueSafe &);

    TaskQueueUnsafe &get_unsafe_queue() { return queue; }

protected:
    void lock() { queuelock.lock(); }
    void unlock() { queuelock.unlock(); }

public:
    typedef typename TaskQueueUnsafe::value_type value_type;
    typedef typename TaskQueueUnsafe::ElementData ElementData;
    typedef TaskQueueUnsafe unsafe_t;

    TaskQueueSafe() {}

    void push_back(value_type *elem) {
        ScopedLockHolder hold(queuelock);
        queue.push_back(elem);
    }

    void push_front(value_type *elem) {
        ScopedLockHolder hold(queuelock);
        queue.push_front(elem);
    }

    // takes ownership of input list
    void push_front_list(TaskQueueUnsafe &list) {
        ScopedLockHolder hold(queuelock);
        queue.push_front_list(list);
    }

    bool pop_front(value_type * &elem) {
        ScopedLockHolder hold(queuelock);
        return queue.pop_front(elem);
    }

    bool pop_back(value_type * &elem) {
        if (queue.empty())
            return false;
        ScopedLockHolder hold(queuelock);
        return queue.pop_back(elem);
    }

    bool try_steal(value_type * &elem) {
        ScopedLockHolderTry hold(queuelock);
        if (!hold.success)
            return false;
        return queue.pop_back(elem);
    }

    void swap(TaskQueueUnsafe &rhs) {
        ScopedLockHolder hold(queuelock);
        queue.swap(rhs);
    }

    bool empty() { 
        return queue.empty();
    }

    bool empty_safe() { 
        ScopedLockHolder hold(queuelock);
        return queue.empty();
    }
};



template<typename TaskQueueType>
class TaskQueueExclusive {
    typedef typename TaskQueueType::value_type value_type;

private:
    TaskQueueType &tq;

    TaskQueueExclusive(const TaskQueueExclusive &);
    const TaskQueueExclusive &operator=(const TaskQueueExclusive &);

public:
    TaskQueueExclusive(TaskQueueType &tq_) : tq(tq_) {
        tq.lock();
    }
    ~TaskQueueExclusive() {
        tq.unlock();
    }
    void push_back(value_type *elem) { tq.get_unsafe_queue().push_back(elem); }
    void swap(typename TaskQueueType::unsafe_t &rhs) { tq.get_unsafe_queue().swap(rhs); }
    bool empty() { return tq.get_unsafe_queue().empty(); }
};

} // namespace detail

} // namespace sg

#endif // SG_TASKQUEUESAFE_HPP_INCLUDED
