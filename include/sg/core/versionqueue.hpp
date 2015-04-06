#ifndef SG_VERSIONQUEUE_HPP_INCLUDED
#define SG_VERSIONQUEUE_HPP_INCLUDED

#include "sg/core/types.hpp"
#include "sg/core/orderedvec.hpp"
#include "sg/core/spinlock.hpp"
#include "sg/platform/atomic.hpp"
#include <limits>

namespace sg {

template<typename Options> class TaskBase;
template<typename Options> class TaskExecutor;

template <typename Options>
class VersionQueue {
    template<typename> friend class VersionQueueExclusive;
private:
    typedef typename Options::version_type version_type;
    typedef typename Options::WaitListType WaitListType;
    typedef typename WaitListType::unsafe_t TaskQueueUnsafe;
    typedef elem_t<version_type, TaskQueueUnsafe> vecelem_t;
    typedef typename Types<Options>::template deque_t<vecelem_t>::type elemdeque_t;
    typedef ordered_vec_t< elemdeque_t, version_type, TaskQueueUnsafe> versionmap_t;

    // lock that must be held during usage of the listener list, and when unlocking
    SpinLock version_listener_spinlock;
    // version listeners, per version
    versionmap_t version_listeners;

    struct DependenciesNotSolvedPredicate {
        bool operator()(TaskBase<Options> *elem) {
            return !elem->are_dependencies_solved_or_notify();
        }
    };

protected:
    SpinLock &get_lock() { return version_listener_spinlock; }
    void add_version_listener(TaskBase<Options> *task, version_type version) {
        version_listeners[version].push_back(task);
    }

public:
    void notify_version_listeners(TaskQueueUnsafe &woken, version_type version) {

        for (;;) {
            TaskQueueUnsafe list;
            {
                SpinLockScoped hold(version_listener_spinlock);

                // return if there are no version listeners
                if (version_listeners.empty())
                    return;

                // return if next version listener is for future version

                if ((version_type)(version - version_listeners.first_key()) >= std::numeric_limits<version_type>::max() / 2)
                    return;

                list = version_listeners.pop_front();
            }

            // Note that a version is increased while holding a lock, and adding a
            // version listener requires holding the same lock. Hence, it is not
            // possible to add a listener for an old version here.
            // The version number is already increased when we wake tasks.

            // iterate through list and remove elements that are not ready

            list.erase_if(DependenciesNotSolvedPredicate());

            if (!list.empty())
                woken.push_front_list(list);
        }
    }
};

template<typename Options>
class VersionQueueExclusive {
    typedef typename Options::version_type version_type;
private:
    VersionQueue<Options> &queue;
    SpinLockScoped lock;
public:
    VersionQueueExclusive(VersionQueue<Options> &queue_) : queue(queue_), lock(queue.get_lock()) {}
    void add_version_listener(TaskBase<Options> *task, version_type version) {
        queue.add_version_listener(task, version);
    }
};

} // namespace sg

#endif // SG_VERSIONQUEUE_HPP_INCLUDED
