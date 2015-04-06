#ifndef SG_HANDLE_HPP_INCLUDED
#define SG_HANDLE_HPP_INCLUDED

#include "sg/core/types.hpp"
#include "sg/platform/atomic.hpp"
#include "sg/core/spinlock.hpp"
#include <cassert>
#include <limits>
#include <string>

namespace sg {

template<typename Options> class Handle;
template<typename Options> class HandleBase;
template<typename Options> class SchedulerVersion;
template<typename Options> class TaskBase;
template<typename Options> class VersionQueue;
template<typename Options> class VersionQueueExclusive;

namespace detail {

template<typename Options> class TaskQueueExclusive;

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
    void set_name(const char *name_) { name = name_; }
    std::string get_name() const { return name; }
};

// ============================================================================
// Option: Global ID for each handle
// ============================================================================
template<typename Options, typename T = typename Options::HandleId> class Handle_GlobalId;

template<typename Options>
class Handle_GlobalId<Options, typename Options::Disable> {};

template<typename Options>
class Handle_GlobalId<Options, typename Options::Enable> {
    typedef typename Options::handleid_type handleid_type;
private:
    handleid_type id;
public:

    Handle_GlobalId() {
        static handleid_type global_handle_id = 0;
        id = Atomic::increase_nv(&global_handle_id);
    }

    handleid_type get_global_id() const { return id; }
};

// ============================================================================
// Option: Contributions
// ============================================================================
template<typename Options, typename T = typename Options::Contributions> class Handle_Contributions;

template<typename Options>
class Handle_Contributions<Options, typename Options::Disable> {
public:
    static size_t apply_contribution() { return 0; }
    static void apply_old_contribs() {}
};

template<typename Options>
class Handle_Contributions<Options, typename Options::Enable>
{
    typedef typename Options::ContributionType Contribution;
private:
        Contribution *contrib;
        SpinLock lock_contrib;
    
public:
        Handle_Contributions() : contrib(0) {}
    
        // run by contrib-supporting task when kernel is finished
        // i.e. may be run from other threads at any time
        // destination may be unlocked (was locked when contribution created,
        // but  lock may have been released since then)
        // lock, (unlock, lock)*, createContrib, (unlock, lock)*, [unlock]
        void add_contribution(Contribution *c) {
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
    
        void apply_old_contribs() {
            // Need to lock here if there are several readers following,
            // or one of them will merge while the other starts reading.
            SpinLockScoped contribLock(lock_contrib);
            Contribution *c = Atomic::swap(&contrib, (Contribution *) 0);
            if (c == 0)
                return;
            Contribution::apply_and_free(c);
        }
    
        // Return current contribution, or 0 if no contribution is attached
        // Use to reuse an old buffer instead of creating a new one
        Contribution *get_contribution() {
            return Atomic::swap(&contrib, (Contribution *) 0);
        }
    
        // for debug only, not thread safe.
        bool has_contrib() {
            return contrib != 0;
        }
};

// ============================================================================
// Option: Lockable
// ============================================================================
template<typename Options, typename T = typename Options::Lockable> class Handle_Lockable;

template<typename Options>
class Handle_Lockable<Options, typename Options::Disable> {
    typedef typename Options::version_type version_type;
    typedef typename Options::WaitListType TaskQueue;
    typedef typename TaskQueue::unsafe_t TaskQueueUnsafe;

public:
    version_type increase_current_version(TaskQueueUnsafe &woken) {
        HandleBase<Options> *this_(static_cast<HandleBase<Options> *>(this));
        version_type ver = Atomic::increase_nv(&this_->version);
        this_->version_queue.notify_version_listeners(woken, this_->version);
        return ver;
    }
};

template<typename Options>
class Handle_Lockable<Options, typename Options::Enable> {
    typedef typename Options::version_type version_type;
    typedef typename Options::lockcount_type lockcount_type;
    typedef typename Options::WaitListType TaskQueue;
    typedef typename TaskQueue::unsafe_t TaskQueueUnsafe;

private:
    // queue of tasks waiting for the lock
    TaskQueue lock_listener_list;
protected:
    // If object is locked or not
    lockcount_type available;

    // Notify lock listeners when the lock is released
    void notify_lock_listeners(TaskQueueUnsafe &woken) {

        TaskQueueUnsafe wake;
        {
            TaskQueueExclusive<TaskQueue> list(lock_listener_list);
            if (list.empty())
                return;

            list.swap(wake);
        }

        woken.push_front_list(wake);
    }

public:
    Handle_Lockable() : available(1) {}

    bool get_lock(lockcount_type required) {
        const lockcount_type ver(Atomic::add_nv(&available, -required));
        if (ver >= 0)
            return true;

        // someone else got in between: revert
        Atomic::add_nv(&available, required);
        return false;
    }

    bool get_lock_or_notify(lockcount_type required, TaskBase<Options> *task) {
        if (required <= available) {
            if (get_lock(required))
                return true;
        }

        // couldn't check out lock -- add to lock listener list
        TaskQueueExclusive<TaskQueue> list(lock_listener_list);

        // we have to make sure lock was not released here, or our listener is never woken.
        if (get_lock(required))
            return true;

        list.push_back(task);

        return false;
    }

    void release_lock(lockcount_type required, TaskQueueUnsafe &woken) {
        Atomic::add_nv(&available, required);
        notify_lock_listeners(woken);
    }

    // for contributions that haven't actually got the lock
    version_type increase_current_version_no_unlock(TaskQueueUnsafe &woken) {
        Handle<Options> *this_(static_cast<Handle<Options> *>(this));

        version_type ver = Atomic::increase_nv(&this_->version);
        this_->version_queue.notify_version_listeners(woken, this_->get_current_version());
        return ver;
    }

    version_type increase_current_version_unlock(lockcount_type required, TaskQueueUnsafe &woken) {
        Atomic::add_nv(&available, required);
        // (someone else can grab the lock here. that is ok.)
        version_type ver = increase_current_version_no_unlock(woken);
        notify_lock_listeners(woken);
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
    typedef typename Options::version_type version_type;

private:
    // Not copyable -- there must be only one data handle per data.
    HandleBase(const HandleBase &);
    const HandleBase &operator=(const HandleBase &);

public:

    // current version of this handle
    version_type version;
    // queue of tasks waiting for future versions of this handle
    VersionQueue<Options> version_queue;
    // next required version for each access type
    SchedulerVersion<Options> required_version;

    HandleBase() : version(0) {}
    ~HandleBase() {
//        if (version != required_version.next_version()-1)
//            std::cerr<<"ver=" << version << " req=" << required_version.next_version()-1 << std::endl;
//        assert(version == required_version.next_version()-1);
    }

    version_type get_current_version() { return version; }
    version_type next_version() { return required_version.next_version(); }

    version_type schedule(int type) {
        return required_version.schedule(type);
    }

    bool is_version_available_or_notify(TaskBase<Options> *task, version_type required_version_) {

        // check if required version is available
        if ((version_type)(version - required_version_) < std::numeric_limits<version_type>::max() / 2)
            return true;

        // the version may become available here; lock and check again

        VersionQueueExclusive<Options> queue(version_queue);
        if ((version_type)(version - required_version_) < std::numeric_limits<version_type>::max() / 2)
            return true;

        queue.add_version_listener(task, required_version_);

        return false;
    }
};

// export Options::HandleType as Handle (default: HandleBase<Options>)
template<typename Options> class Handle : public Options::HandleType {};

// Resource requires Lockable
template<typename Options> class Resource : public Handle<Options> {
public:
    typedef typename Options::lockcount_type lockcount_type;
    Resource(lockcount_type available_ = 1) { this->available = available_; }
};

} // namespace sg

#endif // SG_HANDLE_HPP_INCLUDED
