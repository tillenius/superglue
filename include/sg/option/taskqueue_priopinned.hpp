#ifndef SG_TASKQUEUE_PRIOPINNED_HPP_INCLUDED
#define SG_TASKQUEUE_PRIOPINNED_HPP_INCLUDED

#include "sg/core/taskqueueunsafe.hpp"
#include "sg/core/taskqueuesafe.hpp"

template<typename Options> class TaskBase;

namespace sg {

namespace detail {

template<typename Options>
class TaskQueuePrioPinnedUnsafe {
protected:
    TaskQueueDefaultUnsafe<Options> highprio;
    TaskQueueDefaultUnsafe<Options> lowprio;
    TaskQueueDefaultUnsafe<Options> pinned;

public:
    struct ElementData : public TaskQueueDefaultUnsafe<Options>::ElementData {
        bool is_prioritized;
        int pinned_to;
        ElementData() : is_prioritized(false), pinned_to(-1) {}
        int get_location() const { return pinned_to; }
    };

    typedef TaskBase<Options> value_type;

    TaskQueuePrioPinnedUnsafe() {}

    void push_back(TaskBase<Options> *elem) {
        if (elem->pinned_to != -1)
            pinned.push_back(elem);
        else if (elem->is_prioritized)
            highprio.push_back(elem);
        else
            lowprio.push_back(elem);
    }

    void push_front(TaskBase<Options> *elem) {
        if (elem->pinned_to != -1)
            pinned.push_front(elem);
        else if (elem->is_prioritized)
            highprio.push_front(elem);
        else
            lowprio.push_front(elem);
    }

    // takes ownership of input list
    void push_front_list(TaskQueuePrioPinnedUnsafe &rhs) {
        pinned.push_front_list(rhs.pinned);
        highprio.push_front_list(rhs.highprio);
        lowprio.push_front_list(rhs.lowprio);
    }

    bool pop_front(TaskBase<Options> * &elem) {
        if (highprio.pop_front(elem))
            return true;
        if (pinned.pop_front(elem))
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
        pinned.visit(visitor);
        lowprio.visit(visitor);
    }

    template<typename UnaryPredicate>
    void erase_if(UnaryPredicate pred) {
        highprio.erase_if(pred);
        pinned.erase_if(pred);
        lowprio.erase_if(pred);
    }

    bool empty() {
        return lowprio.empty() && highprio.empty() && pinned.empty();
    }

    void swap(TaskQueuePrioPinnedUnsafe<Options> &rhs) {
        std::swap(highprio, rhs.highprio);
        std::swap(pinned, rhs.pinned);
        std::swap(lowprio, rhs.lowprio);
    }

    size_t size() const {
        return highprio.size() + lowprio.size() + pinned.size();
    }
};

} // namespace detail

template<typename Options>
class TaskQueuePrioPinned : public detail::TaskQueueSafe< detail::TaskQueuePrioPinnedUnsafe<Options>,
                                                          detail::QueueSpinLocked > {};

} // namespace sg

#endif // SG_TASKQUEUE_PRIOPINNED_HPP_INCLUDED
