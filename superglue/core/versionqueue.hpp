#ifndef __VERSIONQUEUE_HPP_
#define __VERSIONQUEUE_HPP_

#include "platform/spinlock.hpp"
#include "platform/atomic.hpp"

template<typename Options>
class VersionQueue {
    template<typename> friend class VersionQueueExclusive;
private:
    typedef typename Types<Options>::versionmap_t versionmap_t;
    typedef typename Options::version_t version_t;
    typedef typename Options::TaskQueueUnsafeType TaskQueueUnsafe;

    // lock that must be held during usage of the listener list, and when unlocking
    SpinLock versionListenerSpinLock;
    // version listeners, per version
    versionmap_t versionListeners;

    struct DependenciesNotSolvedPredicate {
        bool operator()(TaskBase<Options> *elem) {
            return !elem->areDependenciesSolvedOrNotify();
        }
    };

    static void checkDependencies(TaskQueueUnsafe &list, typename Options::LazyDependencyChecking) {}
    static void checkDependencies(TaskQueueUnsafe &list, typename Options::EagerDependencyChecking) {
        list.erase_if(DependenciesNotSolvedPredicate());
    }

protected:
    SpinLock &getLock() { return versionListenerSpinLock; }
    void addVersionListener(TaskBase<Options> *task, version_t version) {
        versionListeners[version].push_back(task);
    }

public:
    void notifyVersionListeners(TaskExecutor<Options> &taskExecutor, version_t version) {

        for (;;) {
            TaskQueueUnsafe list;
            {
                SpinLockScoped verListenerLock(versionListenerSpinLock);

                // return if there are no version listeners
                if (versionListeners.empty())
                    return;

                // return if next version listener is for future version
                if (versionListeners.first_key() > version)
                    return;

                list = versionListeners.pop_front();
            }

            // Note that a version is increased while holding a lock, and adding a
            // version listener requires holding the same lock. Hence, it is not
            // possible to add a listener for an old version here.
            // The version number is already increased when we wake tasks.

            // iterate through list and remove elements that are not ready

            checkDependencies(list, typename Options::DependencyChecking());

            if (!list.empty()) {
                taskExecutor.getTaskQueue().push_front_list(list);
                taskExecutor.getThreadManager().signalNewWork();
            }
        }
    }
};

template<typename Options>
class VersionQueueExclusive {
    typedef typename Options::version_t version_t;
private:
    VersionQueue<Options> &queue;
    SpinLockScoped lock;
public:
    VersionQueueExclusive(VersionQueue<Options> &queue_) : queue(queue_), lock(queue.getLock()) {}
    void addVersionListener(TaskBase<Options> *task, version_t version) {
        queue.addVersionListener(task, version);
    }
};

#endif // __VERSIONQUEUE_HPP_
