#ifndef __TASKQUEUE_PRIO_HPP__
#define __TASKQUEUE_PRIO_HPP__

#include "core/types.hpp"
#include "platform/spinlock.hpp"
#include "core/taskqueue.hpp"
#include "core/taskqueueutil.hpp"
#include "core/log.hpp"

#include <stdint.h>

template<typename, typename> class Log_DumpState;
template<typename Options> class TaskBase;
template<typename Options> class Log;

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
class TaskQueuePrioUnsafe {
protected:
    TaskQueueDefaultUnsafe<Options> highprio;
    TaskQueueDefaultUnsafe<Options> lowprio;

public:
    struct ElementData {
        uintptr_t nextPrev;
        bool is_prioritized;
        ElementData() : nextPrev(0), is_prioritized(false) {}
    };

    typedef TaskBase<Options> value_type;

    TaskQueuePrioUnsafe() {}

    void push_back(TaskBase<Options> *elem) {
        if (elem->is_prioritized)
            highprio.push_back(elem);
        else
            lowprio.push_back(elem);
    }

    void push_front(TaskBase<Options> *elem) {
        if (elem->is_prioritized)
            highprio.push_front(elem);
        else
            lowprio.push_front(elem);
    }

    // takes ownership of input list
    void push_front_list(TaskQueuePrioUnsafe &rhs) {
        highprio.push_front_list(rhs.highprio);
        lowprio.push_front_list(rhs.lowprio);
    }

    bool pop_front(TaskBase<Options> * &elem) {
        if (highprio.pop_front(elem))
            return true;
        return lowprio.pop_front(elem);
    }

    bool pop_back(TaskBase<Options> * &elem) {
        if (highprio.pop_back(elem))
            return true;
        return lowprio.pop_back(elem);
    }

    template<typename Visitor>
    void visit(Visitor &visitor) {
        highprio.visit(visitor);
        lowprio.visit(visitor);
    }

    template<typename UnaryPredicate>
    void erase_if(UnaryPredicate pred) {
        highprio.erase_if(pred);
        lowprio.erase_if(pred);
    }

    bool empty() {
        return lowprio.empty() && highprio.empty();
    }

    void swap(TaskQueuePrioUnsafe<Options> &rhs) {
        std::swap(highprio, rhs.highprio);
        std::swap(lowprio, rhs.lowprio);
    }

    size_t size() const {
        return highprio.size() + lowprio.size();
    }
};

#endif // __TASKQUEUE_HPP__
