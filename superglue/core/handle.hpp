#ifndef __HANDLE_HPP__
#define __HANDLE_HPP__

#include "core/types.hpp"
#include "core/versionqueue.hpp"
#include "core/taskqueue.hpp"
#include "platform/spinlock.hpp"
#include "platform/atomic.hpp"
#include "core/log.hpp"
#include <cassert>

template<typename Options> class Handle;
template<typename Options> class TaskBase;
template<typename Options> class TaskExecutor;
template<typename Options> class SchedulerVersion;
template<typename Options> class Log;

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
private:
    size_t id;
public:

    Handle_GlobalId() {
        static size_t global_handle_id = 0;
        id = Atomic::increase_nv(&global_handle_id);
    }

    size_t getGlobalId() const { return id; }
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
public:
    void increaseCurrentVersionNoUnlock(TaskExecutor<Options> &taskExecutor) { increaseCurrentVersion(taskExecutor); }
    void increaseCurrentVersion(TaskExecutor<Options> &taskExecutor) {
        Handle<Options> *this_(static_cast<Handle<Options> *>(this));
        this_->increaseCurrentVersionImpl();
        this_->versionQueue.notifyVersionListeners(taskExecutor, this_->version);
    }
};

template<typename Options>
class Handle_Lockable<Options, typename Options::Enable> {
    template<typename, typename> friend class Handle_Contributions;
private:
    // queue of tasks waiting for the lock
    TaskQueue<Options> lockListenerList;
    // If object is locked or not
    SpinLockAtomic dataLock;

    // Notify lock listeners when the lock is released
    void notifyLockListeners(TaskExecutor<Options> &taskExecutor) {

        if (!wakeLockQueue(taskExecutor))
            return;

        taskExecutor.getThreadManager().signalNewWork();
    }

    bool wakeLockQueue(TaskExecutor<Options> &taskExecutor) {
        TaskQueueUnsafe<Options> wake;
        lockListenerList.swap(wake);
        if (!wake.gotWork())
            return false;

        taskExecutor.getTaskQueue().push_front_list(wake);
        return true;
    }

public:
    Handle_Lockable() {}

    bool getLock() {
        return dataLock.try_lock();
    }

    bool getLockOrNotify(TaskBase<Options> *task) {
        if (dataLock.try_lock())
            return true;

        TaskQueueExclusive<Options> list(lockListenerList);

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
    void increaseCurrentVersionNoUnlock(TaskExecutor<Options> &taskExecutor) {
        Handle<Options> *this_(static_cast<Handle<Options> *>(this));

        this_->increaseCurrentVersionImpl();
        this_->versionQueue.notifyVersionListeners(taskExecutor, this_->getCurrentVersion());
    }

    void increaseCurrentVersion(TaskExecutor<Options> &taskExecutor) {
        Handle<Options> *this_(static_cast<Handle<Options> *>(this));

        // first check if we owned the lock before version is increased
        // (otherwise someone else might grab the lock in between)
        if (dataLock.is_locked()) {
            dataLock.unlock();
            // (someone else can snatch the lock here. that is ok.)
            Atomic::increase(&this_->version);
            this_->versionQueue.notifyVersionListeners(taskExecutor, this_->version);
            notifyLockListeners(taskExecutor);
        }
        else {
            Atomic::increase(&this_->version);
            this_->versionQueue.notifyVersionListeners(taskExecutor, this_->version);
        }
    }
};

} // namespace detail

// ============================================================================
// Handle
// ============================================================================
template<typename Options>
class HandleDefault
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
    HandleDefault(const HandleDefault &);
    const HandleDefault &operator=(const HandleDefault &);

public:

    // current version of this handle
    version_t version;
    // queue of tasks waiting for future versions of this handle
    VersionQueue<Options> versionQueue;
    // next required version for each access type
    SchedulerVersion<Options> requiredVersion;

    void increaseCurrentVersionImpl() {
        Atomic::increase(&version);
    }

    HandleDefault() : version(0) {}
    ~HandleDefault() {
        if (version != requiredVersion.nextVersion()-1) {
            std::cerr<<"ver=" << version << " req=" << requiredVersion.nextVersion()-1 << std::endl;
            raise(SIGINT);
        }
        assert(version == requiredVersion.nextVersion()-1);
    }

    version_t getCurrentVersion() { return version; }
    version_t nextVersion() { return requiredVersion.nextVersion(); }

    version_t schedule(int type) {
        return requiredVersion.schedule(type);
    }

    bool isVersionAvailableOrNotify(TaskBase<Options> *listener, version_t requiredVersion_) {

        // check if required version is available
        if ( (int) (version - requiredVersion_) >= 0)
            return true;

        // the version may become available here; lock and check again

        VersionQueueExclusive<Options> queue(versionQueue);
        if ( (int) (version - requiredVersion_) >= 0)
            return true;

        queue.addVersionListener(listener, requiredVersion_);
        return false;
    }

};

template<typename Options> class Handle : public Options::HandleType {};

#endif // __HANDLE_HPP__
