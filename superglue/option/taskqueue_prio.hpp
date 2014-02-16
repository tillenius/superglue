#ifndef __TASKQUEUE_PRIO_HPP__
#define __TASKQUEUE_PRIO_HPP__

#include "core/taskqueueunsafe.hpp"
#include "core/taskqueuesafe.hpp"

template<typename Options> class TaskBase;

namespace detail {

template<typename Options>
class TaskQueuePrioUnsafe {
protected:
    TaskQueueDefaultUnsafe<Options> highprio;
    TaskQueueDefaultUnsafe<Options> lowprio;

public:
    struct ElementData : public TaskQueueDefaultUnsafe<Options>::ElementData {
        bool is_prioritized;
        ElementData() : is_prioritized(false) {}
    };

    typedef TaskBase<Options> value_type;
    void init(int num_threads) {}

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
}

template<typename Options>
class TaskQueuePrio : public detail::TaskQueueSafe< detail::TaskQueuePrioUnsafe< Options >,
                                                    detail::QueueSpinLocked > {};

#endif // __TASKQUEUE_PRIO_HPP__
