#ifndef __TASKQUEUE_HPP__
#define __TASKQUEUE_HPP__

#include "core/types.hpp"
#include "platform/spinlock.hpp"
#include "core/log.hpp"

#include <stdint.h>
#include <iostream>

template<typename, typename> class Log_DumpState;
template<typename Options> class TaskBase;
template<typename Options> class Log;

template<typename Options>
class TaskQueueUnsafe<Options, typename Options::Disable>
 : public Types<Options>::taskdeque_t {
    typedef typename Types<Options>::taskdeque_t parent;

public:
    bool pop_front(TaskBase<Options> * &elem) {
        if (parent::empty())
            return false;

        Options::Scheduler::getTask(*this, elem);

        return true;
    }

    bool pop_back(TaskBase<Options> * &elem) {
        if (parent::empty())
            return false;
        return Options::Scheduler::stealTask(*this, elem);
    }

    void push_front_list(TaskQueueUnsafe<Options> &rhs) {
        parent::insert(parent::begin(), rhs.begin(), rhs.end());
    }

    bool empty() {
        return parent::empty();
    }

    template<typename UnaryPredicate>
    void erase_if(UnaryPredicate pred) {
        remove_if(parent::begin(), parent::end(), pred);
    }
};

// TaskQueueUnsafe is a (non-thread-safe) doubly linked list
// (or "xor linked list", see http://en.wikipedia.org/wiki/XOR_linked_list)
//
// To walk forward in the list: next = prev ^ curr->nextPrev
//
// Example: List elements: A, B, C
//
//  0     A     B     C     0
//  A    0^B   A^B   B^0    C

template<typename Options>
class TaskQueueUnsafe<Options, typename Options::Enable> {
protected:
    TaskBase<Options> *first;
    TaskBase<Options> *last;

    template<typename, typename> friend struct Log_DumpState;

public:
    TaskQueueUnsafe() : first(0), last(0) {}

    void push_back(TaskBase<Options> *elem) {
        if (last == 0) {
            elem->nextPrev = 0;
            first = last = elem;
        }
        else {
            last->nextPrev = last->nextPrev ^ reinterpret_cast<uintptr_t>(elem);
            elem->nextPrev = reinterpret_cast<uintptr_t>(last);
            last = elem;
        }
    }

    void push_front(TaskBase<Options> *elem) {
        if (first == 0) {
            elem->nextPrev = 0;
            first = last = elem;
        }
        else {
            first->nextPrev ^= reinterpret_cast<uintptr_t>(elem);
            elem->nextPrev = reinterpret_cast<uintptr_t>(first);
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
            first->nextPrev ^= reinterpret_cast<uintptr_t>(rhs.last);
            rhs.last->nextPrev ^= reinterpret_cast<uintptr_t>(first);
            first = rhs.first;
        }
    }

    bool pop_front(TaskBase<Options> * &elem) {
        if (first == 0)
            return false;

        elem = first;
        first = reinterpret_cast<TaskBase<Options> *>(first->nextPrev);
        if (first == 0)
            last = 0;
        else
            first->nextPrev ^= reinterpret_cast<uintptr_t>(elem);

        return true;
    }

    bool pop_back(TaskBase<Options> * &elem) {
        if (last == 0)
            return false;

        elem = last;
        last = reinterpret_cast<TaskBase<Options> *>(last->nextPrev);
        if (last == 0)
            first = 0;
        else
            last->nextPrev ^= reinterpret_cast<uintptr_t>(elem);

        return true;
    }

    template<typename Visitor>
    void visit(Visitor &visitor) {

        TaskBase<Options> *curr(first);
        TaskBase<Options> *prev(0);
        TaskBase<Options> *next;

        for (;curr != 0;) {
            visitor.visit(curr);
            next = reinterpret_cast<TaskBase<Options> *>(
                reinterpret_cast<uintptr_t>(prev) ^ curr->nextPrev);
            prev = curr;
            curr = next;
        }
    }

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
                reinterpret_cast<uintptr_t>(prev) ^ curr->nextPrev);
            if (next == 0) {
                std::cerr << std::endl;
                return;
            }
            prev = curr;
            curr = next;
        }
    }

    template<typename UnaryPredicate>
    void erase_if(UnaryPredicate pred) {
        if (first == 0)
            return;
        TaskBase<Options> *prev(0);
        TaskBase<Options> *curr(first);

        for (;;) {
            TaskBase<Options> *next = reinterpret_cast<TaskBase<Options> *>(
                reinterpret_cast<uintptr_t>(prev) ^ curr->nextPrev);

            if (pred(curr)) {
                if (prev == 0)
                    first = next;
                else {
                    prev->nextPrev = prev->nextPrev
                        ^ reinterpret_cast<uintptr_t>(curr)
                        ^ reinterpret_cast<uintptr_t>(next);
                }

                if (next == 0) {
                    last = prev;
                    return;
                }
                else {
                    next->nextPrev = next->nextPrev
                        ^ reinterpret_cast<uintptr_t>(curr)
                        ^ reinterpret_cast<uintptr_t>(prev);
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

    bool empty() { return first == 0; }

    void swap(TaskQueueUnsafe &rhs) {
        std::swap(first, rhs.first);
        std::swap(last, rhs.last);
    }

    size_t size() const {
        if (first == 0)
            return 0;

        TaskBase<Options> *curr(first);
        TaskBase<Options> *prev(0);
        TaskBase<Options> *next;

        size_t count = 0;
        for (;;) {
            ++count;
            next = reinterpret_cast<TaskBase<Options> *>(
                reinterpret_cast<uintptr_t>(prev) ^ curr->nextPrev);
            if (next == 0)
                return count;
            prev = curr;
            curr = next;
        }
    }
};

template<typename Options>
class TaskQueue : public TaskQueueUnsafe<Options> {
    template<typename> friend class TaskQueueExclusive;
private:
    SpinLock spinlock;

    TaskQueue(const TaskQueue &);
    const TaskQueue &operator=(const TaskQueue &);

    template<typename, typename> friend struct Log_DumpState;

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

    bool pop_front(TaskBase<Options> * &elem) {
        SpinLockScoped lock(spinlock);
        return TaskQueueUnsafe<Options>::pop_front(elem);
    }

    bool pop_back(TaskBase<Options> * &elem) {
        if (TaskQueueUnsafe<Options>::empty())
            return false;
        SpinLockScoped lock(spinlock);
        return TaskQueueUnsafe<Options>::pop_back(elem);
    }

    bool try_steal(TaskBase<Options> * &elem) {
        SpinLockTryLock lock(spinlock);
        if (!lock.success)
            return false;
        return TaskQueueUnsafe<Options>::pop_back(elem);
    }

    void swap(TaskQueueUnsafe<Options> &rhs) {
        SpinLockScoped lock(spinlock);
        TaskQueueUnsafe<Options>::swap(rhs);
    }

    bool empty_safe() { 
        SpinLockScoped lock(spinlock);
        return TaskQueueUnsafe<Options>::empty();
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
