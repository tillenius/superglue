#ifndef __TASKQUEUE_HPP__
#define __TASKQUEUE_HPP__

#include "core/types.hpp"
#include "platform/spinlock.hpp"
#include "core/log.hpp"

template<typename Options> class TaskBase;
template<typename Options> class Log;

template<typename Options>
class TaskQueueUnsafe<Options, typename Options::Disable>
 : public Types<Options>::taskdeque_t {
    typedef typename Types<Options>::taskdeque_t parent;

public:
    bool get(TaskBase<Options> * &elem) {
        if (parent::empty())
            return false;

        Options::Scheduler::getTask(*this, elem);

        return true;
    }

    bool steal(TaskBase<Options> * &elem) {
        if (parent::empty())
            return false;
        return Options::Scheduler::stealTask(*this, elem);
    }

    void push_front_list(TaskQueueUnsafe<Options> &rhs) {
        parent::insert(parent::begin(), rhs.begin(), rhs.end());
    }

    bool gotWork() {
        return !parent::empty();
    }
};

// TaskQueueUnsafe is a (non-thread-safe) doubly linked list
// (or "subtraction linked list", see http://en.wikipedia.org/wiki/XOR_linked_list)
//
// To walk forward in the list: next = prev + curr->nextPrev
//
// Example: List structure and removal of an element.
//   List elements: A, B, C
//   Algorithm pointers: p (previous), c (current), n (next)
//
//        p     c     n
//  0     A     B     C     0
// A-0   B-0   C-A   0-B    C
//
// update prev->nextPrev:
//          p       c     n
//  0       A       B     C     0
// A-0   B-0-B+C   C-A   0-B    C
//
// update next->nextPrev:
//          p       c       n
//  0       A       B       C     0
// A-0   B-0-B+C   C-A   D-B+B-A  C
//
// result:
//        p    n
//  0     A    C    0
// A-0   C-0  D-A   C

template<typename Options>
class TaskQueueUnsafe<Options, typename Options::Enable> {
protected:
    TaskBase<Options> *first;
    TaskBase<Options> *last;

    template<typename, typename> friend struct detail::Log_DumpState;

public:
    TaskQueueUnsafe() : first(0), last(0) {}

    void push_back(TaskBase<Options> *elem) {
        if (last == 0) {
            elem->nextPrev = 0;
            first = last = elem;
        }
        else {
            last->nextPrev = last->nextPrev + reinterpret_cast<std::ptrdiff_t>(elem);
            elem->nextPrev = -reinterpret_cast<std::ptrdiff_t>(last);
            last = elem;
        }
    }

    void push_front(TaskBase<Options> *elem) {
        if (first == 0) {
            elem->nextPrev = 0;
            first = last = elem;
        }
        else {
            first->nextPrev -= reinterpret_cast<std::ptrdiff_t>(elem);
            elem->nextPrev = reinterpret_cast<std::ptrdiff_t>(first);
            first = elem;
        }
    }

    // takes ownership of input list
    void push_front_list(TaskQueueUnsafe<Options> &rhs) {
        if (first == 0) {
            first = rhs.first;
            last = rhs.last;
        }
        else {
            first->nextPrev -= reinterpret_cast<std::ptrdiff_t>(rhs.last);
            rhs.last->nextPrev += reinterpret_cast<std::ptrdiff_t>(first);
            first = rhs.first;
        }
    }

    bool get(TaskBase<Options> * &elem) {
        if (first == 0)
            return false;

        elem = first;
        first = reinterpret_cast<TaskBase<Options> *>(first->nextPrev);
        if (first == 0)
            last = 0;
        else
            first->nextPrev += reinterpret_cast<std::ptrdiff_t>(elem);

        return true;
    }

    bool steal(TaskBase<Options> * &elem) {
        if (last == 0)
            return false;

        elem = last;
        last = reinterpret_cast<TaskBase<Options> *>(-last->nextPrev);
        if (last == 0)
            first = 0;
        else
            last->nextPrev -= reinterpret_cast<std::ptrdiff_t>(elem);

        return true;
    }

#ifdef DEBUG_TASKQUEUE
    void dump() {
        if (first == 0) {
            std::cerr << "[Empty]" << std::endl;
            return;
        }
        TaskBase<Options> *curr(first);
        TaskBase<Options> *prev(0);
        TaskBase<Options> *next;

        for (;;) {
            std::cerr << curr << " ";
            next = reinterpret_cast<TaskBase<Options> *>(
                reinterpret_cast<std::ptrdiff_t>(prev) + curr->nextPrev);
            if (next == 0) {
                std::cerr << std::endl;
                return;
            }
            prev = curr;
            curr = next;
        }
    }
#endif // DEBUG_TASKQUEUE

    template<typename Pred>
    void erase_if(Pred) {
        if (first == 0)
            return;
        TaskBase<Options> *prev(0);
        TaskBase<Options> *curr(first);

        for (;;) {
            TaskBase<Options> *next = reinterpret_cast<TaskBase<Options> *>(
                reinterpret_cast<std::ptrdiff_t>(prev) + curr->nextPrev);

            if (Pred::test(curr)) {
                if (prev == 0)
                    first = next;
                else {
                    prev->nextPrev = prev->nextPrev
                        - reinterpret_cast<std::ptrdiff_t>(curr)
                        + reinterpret_cast<std::ptrdiff_t>(next);
                }

                if (next == 0) {
                    last = prev;
                    return;
                }
                else {
                    next->nextPrev = next->nextPrev
                        + reinterpret_cast<std::ptrdiff_t>(curr)
                        - reinterpret_cast<std::ptrdiff_t>(prev);
                }

                prev = 0;
                curr = first;
                continue;

            }
            else {
                prev = curr;
                if (next == 0)
                    return;
            }
            curr = next;
        }
    }

    bool gotWork() { return first != 0; }

    void swap(TaskQueueUnsafe &rhs) {
        std::swap(first, rhs.first);
        std::swap(last, rhs.last);
    }
};


template<typename Options>
class TaskQueue : public TaskQueueUnsafe<Options> {
    template<typename> friend class TaskQueueExclusive;
private:
    SpinLock spinlock;

    TaskQueue(const TaskQueue &);
    const TaskQueue &operator=(const TaskQueue &);

    template<typename, typename> friend struct detail::Log_DumpState;

    SpinLock &getLock() { return spinlock; }
    TaskQueueUnsafe<Options> &getUnsafeQueue() { return *this; }

public:
    TaskQueue() {}

    void push_back(TaskBase<Options> *elem) {
        SpinLockScoped lock(spinlock);
        TaskQueueUnsafe<Options>::push_back(elem);
    }

    void push_front(TaskBase<Options> *elem) {
        SpinLockScoped lock(spinlock);
        TaskQueueUnsafe<Options>::push_front(elem);
    }

    // takes ownership of input list
    void push_front_list(TaskQueueUnsafe<Options> &list) {
        SpinLockScoped lock(spinlock);
        TaskQueueUnsafe<Options>::push_front_list(list);
    }

    bool get(TaskBase<Options> * &elem) {
        SpinLockScoped lock(spinlock);
        return TaskQueueUnsafe<Options>::get(elem);
    }

    bool steal(TaskBase<Options> * &elem) {
        if (!TaskQueueUnsafe<Options>::gotWork())
            return false;
        SpinLockScoped lock(spinlock);
        return TaskQueueUnsafe<Options>::steal(elem);
    }

    void swap(TaskQueueUnsafe<Options> &rhs) {
        SpinLockScoped lock(spinlock);
        TaskQueueUnsafe<Options>::swap(rhs);
    }
};

template<typename Options>
class TaskQueueExclusive {
private:
    SpinLockScoped lock;
    TaskQueueUnsafe<Options> &queue;

public:
    TaskQueueExclusive(TaskQueue<Options> &tq) : lock(tq.getLock()), queue(tq.getUnsafeQueue()) {}

    void push_back(TaskBase<Options> *elem) { queue.push_back(elem); }
    void push_front(TaskBase<Options> *elem) { queue.push_front(elem); }
    void push_front_list(TaskQueueUnsafe<Options> &list) { queue.push_front_list(list); }
    bool get(TaskBase<Options> * &elem) { return queue.get(elem); }
    bool steal(TaskBase<Options> * &elem) { return queue.steal(elem); }
    void swap(TaskQueueUnsafe<Options> &rhs) { queue.swap(rhs); }
};

#endif // __TASKQUEUE_HPP__
