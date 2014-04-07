#ifndef SG_TASKQUEUEUNSAFE_HPP_INCLUDED
#define SG_TASKQUEUEUNSAFE_HPP_INCLUDED

#include "sg/core/types.hpp"
#include "sg/core/spinlock.hpp"
#include "sg/core/taskqueuesafe.hpp"

#include <stdint.h>
#include <iostream>

namespace sg {

template<typename Options> class TaskBase;

// TaskQueueUnsafe is a (non-thread-safe) doubly linked list
// (or "xor linked list", see http://en.wikipedia.org/wiki/XOR_linked_list)
//
// To walk forward in the list: next = prev ^ curr->nextPrev
//
// Example: List elements: A, B, C
//
//  0     A     B     C     0
//  A    0^B   A^B   B^0    C

namespace detail {

template<typename Options>
class TaskQueueDefaultUnsafe {
protected:
    TaskBase<Options> *first;
    TaskBase<Options> *last;

public:
    struct ElementData {
        uintptr_t nextPrev;
        static int get_location() { return -1; }
        ElementData() : nextPrev(0) {}
    };

    typedef TaskBase<Options> value_type;

    TaskQueueDefaultUnsafe() : first(0), last(0) {}

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
    void push_front_list(TaskQueueDefaultUnsafe &rhs) {
        if (rhs.first == 0)
            return;
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
            visitor(curr);
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

    void swap(TaskQueueDefaultUnsafe<Options> &rhs) {
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

} // namsepace detail

} // namespace sg

#endif // SG_TASKQUEUEUNSAFE_HPP_INCLUDED
