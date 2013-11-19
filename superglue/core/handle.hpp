#ifndef __HANDLE_HPP__
#define __HANDLE_HPP__

#include "core/types.hpp"
#include "core/versionqueue.hpp"
#include "core/taskqueue.hpp"
#include "platform/spinlock.hpp"
#include "platform/atomic.hpp"
#include <cassert>
#include <limits>

template<typename Options> class HandleBase;
template<typename Options> class TaskBase;
template<typename Options> class TaskExecutor;
template<typename Options> class SchedulerVersion;

namespace detail {

// ============================================================================
// Option: Handle_Name
// ============================================================================
template<typename Options, typename T = typename Options::HandleName> class Handle_Name;

template<typename Options>
class Handle_Name<Options, typename Options::Disable> {};

template<typename Options>
class Handle_Name<Options, typename Options::Enable> {
private:
    std::string name;
public:
    void setName(const char *name_) { name = name_; }
    std::string getName() const { return name; }
};

// ============================================================================
// Option: Global ID for each handle
// ============================================================================
template<typename Options, typename T = typename Options::HandleId> class Handle_GlobalId;

template<typename Options>
class Handle_GlobalId<Options, typename Options::Disable> {};

template<typename Options>
class Handle_GlobalId<Options, typename Options::Enable> {
    typedef typename Options::handleid_t handleid_t;
private:
    handleid_t id;
public:

    Handle_GlobalId() {
        static handleid_t global_handle_id = 0;
        id = Atomic::increase_nv(&global_handle_id);
    }

    handleid_t getGlobalId() const { return id; }
};

// ============================================================================
// Option: Contributions
// ============================================================================
template<typename Options, typename T = typename Options::Contributions> class Handle_Contributions;

template<typename Options>
class Handle_Contributions<Options, typename Options::Disable> {
public:
    static size_t applyContribution() { return 0; }
    static void applyOldContributionsBeforeRead() {}
};

template<typename Options>
class Handle_Contributions<Options, typename Options::Enable>
{
    typedef typename Options::ContributionType Contribution;
private:
    Contribution *contrib;
    SpinLock lockContrib;

public:
    Handle_Contributions() : contrib(0) {}

    // run by contrib-supporting task when kernel is finished
    // i.e. may be run from other threads at any time
    // destination may be unlocked (was locked when contribution created,
    // but  lock may have been released since then)
    // lock, (unlock, lock)*, createContrib, (unlock, lock)*, [unlock]
    void addContribution(Contribution *c) {
        for (;;) {
            // expect zero
            Contribution *old = Atomic::cas(&contrib, (Contribution *) 0, c);
            if (old == 0)
                return;

            // get old, write zero
            old = Atomic::swap(&contrib, (Contribution *) 0);
            if (old != 0)
                c->merge(old);
        }
    }

    void applyOldContributionsBeforeRead() {
        // Need to lock here if there are several readers following,
        // or one of them will merge while the other starts reading.
        SpinLockScoped contribLock(lockContrib);
        Contribution *c = Atomic::swap(&contrib, (Contribution *) 0);
        if (c == 0)
            return;
        Contribution::applyAndFree(c);
    }

    // Return current contribution, or 0 if no contribution is attached
    // Use to reuse an old buffer instead of creating a new one
    Contribution *getContribution() {
        return Atomic::swap(&contrib, (Contribution *) 0);
    }

    // for debug only, not thread safe.
    bool hasContrib() {
        return contrib != 0;
    }
};

// ============================================================================
// Option: Lockable
// ============================================================================
template<typename Options, typename T = typename Options::Lockable> class Handle_Lockable;

template<typename Options>
class Handle_Lockable<Options, typename Options::Disable> {
    typedef typename Options::version_t version_t;
public:
    version_t increaseCurrentVersionNoUnlock(TaskExecutor<Options> &taskExecutor) { 
        return increaseCurrentVersion(taskExecutor);
    }
    version_t increaseCurrentVersion(TaskExecutor<Options> &taskExecutor) {
        HandleBase<Options> *this_(static_cast<HandleBase<Options> *>(this));
        version_t ver = Atomic::increase_nv(&this_->version);
        this_->versionQueue.notifyVersionListeners(taskExecutor, this_->version);
        return ver;
    }
};

template<typename Options>
class Handle_Lockable<Options, typename Options::Enable> {
    template<typename, typename> friend class Handle_Contributions;
    typedef typename Options::version_t version_t;
    typedef typename Options::TaskQueueUnsafeType TaskQueueUnsafe;
private:
    // queue of tasks waiting for the lock
    TaskQueueSafe<TaskQueueUnsafe> lockListenerList;
    // If object is locked or not
    SpinLockAtomic dataLock;

    // Notify lock listeners when the lock is released
    void notifyLockListeners(TaskExecutor<Options> &taskExecutor) {

        TaskQueueUnsafe wake;
        {
            TaskQueueExclusive<typename Options::TaskQueueUnsafeType> list(lockListenerList);
            if (list.empty())
                return;

            list.swap(wake);
        }

        taskExecutor.getTaskQueue().push_front_list(wake); // destroys wake

        taskExecutor.getThreadManager().signalNewWork();
    }

public:
    Handle_Lockable() {}

    bool getLock() {
        return dataLock.try_lock();
    }

    bool getLockOrNotify(TaskBase<Options> *task) {
        if (dataLock.try_lock())
            return true;

        TaskQueueExclusive<typename Options::TaskQueueUnsafeType> list(lockListenerList);

        // we have to make sure lock was not released here, or our listener is never woken.
        if (dataLock.try_lock())
            return true;

        list.push_back(task);

        return false;
    }

    void releaseLock(TaskExecutor<Options> &taskExecutor) {
        dataLock.unlock();
        notifyLockListeners(taskExecutor);
    }

    // for contributions that haven't actually got the lock
    version_t increaseCurrentVersionNoUnlock(TaskExecutor<Options> &taskExecutor) {
        Handle<Options> *this_(static_cast<Handle<Options> *>(this));

        version_t ver = Atomic::increase_nv(&this_->version);
        this_->versionQueue.notifyVersionListeners(taskExecutor, this_->getCurrentVersion());
        return ver;
    }

    version_t increaseCurrentVersion(TaskExecutor<Options> &taskExecutor) {
        HandleBase<Options> *this_(static_cast<HandleBase<Options> *>(this));

        // first check if we owned the lock before version is increased
        // (otherwise someone else might grab the lock in between)
        version_t ver;
        if (dataLock.is_locked()) {
            dataLock.unlock();
            // (someone else can snatch the lock here. that is ok.)
            ver = Atomic::increase_nv(&this_->version);
            this_->versionQueue.notifyVersionListeners(taskExecutor, this_->version);
            notifyLockListeners(taskExecutor);
        }
        else {
            ver = Atomic::increase_nv(&this_->version);
            this_->versionQueue.notifyVersionListeners(taskExecutor, this_->version);
        }
        return ver;
    }
};

} // namespace detail

// ============================================================================
// Handle
// ============================================================================
template<typename Options>
class HandleBase
  : public detail::Handle_Lockable<Options>,
    public detail::Handle_Contributions<Options>,
    public detail::Handle_GlobalId<Options>,
    public detail::Handle_Name<Options>
{
    template<typename, typename> friend class Handle_Lockable;
    template<typename, typename> friend class Handle_Contributions;

    typedef typename Options::version_t version_t;

private:
    // Not copyable -- there must be only one data handle per data.
    HandleBase(const HandleBase &);
    const HandleBase &operator=(const HandleBase &);

public:

    // current version of this handle
    version_t version;
    // queue of tasks waiting for future versions of this handle
    VersionQueue<Options> versionQueue;
    // next required version for each access type
    SchedulerVersion<Options> requiredVersion;

    HandleBase() : version(0) {}
    ~HandleBase() {
        if (version != requiredVersion.nextVersion()-1)
            std::cerr<<"ver=" << version << " req=" << requiredVersion.nextVersion()-1 << std::endl;
        assert(version == requiredVersion.nextVersion()-1);
    }

    version_t getCurrentVersion() { return version; }
    version_t nextVersion() { return requiredVersion.nextVersion(); }

    version_t schedule(int type) {
        return requiredVersion.schedule(type);
    }

    bool isVersionAvailableOrNotify(TaskBase<Options> *task, version_t requiredVersion_) {

        // check if required version is available
        if ( (version_t) (version - requiredVersion_) < std::numeric_limits<version_t>::max()/2)
            return true;

        // the version may become available here; lock and check again

        VersionQueueExclusive<Options> queue(versionQueue);
        if ( (version_t) (version - requiredVersion_) < std::numeric_limits<version_t>::max()/2)
            return true;

        queue.addVersionListener(task, requiredVersion_);
        return false;
    }
};

// export Options::HandleType as Handle (default: HandleBase<Options>)
template<typename Options> class Handle : public Options::HandleType {};

#endif // __HANDLE_HPP__
