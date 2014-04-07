#ifndef SG_ACCESS_HPP_INCLUDED
#define SG_ACCESS_HPP_INCLUDED

#include <algorithm>
#include <cstring>

namespace sg {

template<typename Options> class Access;
template<typename Options> class Handle;
template<typename Options> class TaskBase;
template<typename Options> class TaskExecutor;

namespace detail {

// ============================================================================
// Option Contributions
// ============================================================================
template<typename Options, typename T = typename Options::Contributions> class Access_Contributions;

template<typename Options>
class Access_Contributions<Options, typename Options::Disable> {
public:
    static bool use_contrib() { return false; }
    static void set_use_contrib(bool ) {}
};

template<typename Options>
class Access_Contributions<Options, typename Options::Enable> {
private:
    bool use_contrib_flag;
public:
    typedef typename Options::ContributionType Contribution;

    Access_Contributions() : use_contrib_flag(false) {}
    void set_use_contrib(bool value) { use_contrib_flag = value; }
    bool use_contrib() const { return use_contrib_flag; }
    void add_contribution(Contribution c) {
        this->get_handle()->add_contribution(c);
    }
};

// ============================================================================
// Option Lockable
// ============================================================================
template<typename Options, typename T = typename Options::Lockable> class Access_Lockable;

template<typename Options>
class Access_Lockable<Options, typename Options::Disable> {
    typedef typename Options::version_t version_t;
    typedef typename Options::lockcount_t lockcount_t;
public:
    static bool get_lock() { return true; }
    static bool needs_lock() { return false; }
    static void release_lock(TaskExecutor<Options> &) {}
    static bool get_lock_or_notify(TaskBase<Options> *) { return true; }
    static void set_required_quantity(lockcount_t required_) {}
    version_t finished(TaskExecutor<Options> &taskExecutor) {
        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        return this_->handle->increase_current_version(taskExecutor);
    }
};

template<typename Options>
class Access_Lockable<Options, typename Options::Enable> {
    typedef typename Options::lockcount_t lockcount_t;
    typedef typename Options::version_t version_t;
private:
    lockcount_t required;
public:
    Access_Lockable() : required(0) {}

    // Check if lock is available, or add a listener
    bool get_lock_or_notify(TaskBase<Options> *task) const {
        if (required == 0)
            return true;

        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        return this_->handle->get_lock_or_notify(required, task);
    }

    // Get lock if its free, or return false.
    // Low level interface to lock several objects simultaneously
    bool get_lock() const {
        if (required == 0)
            return true;

        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        return this_->handle->get_lock(required);
    }

    // Low level interface to unlock when failed to lock several objects
    void release_lock(TaskExecutor<Options> &taskExecutor) const {
        if (required == 0)
            return;

        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        this_->handle->release_lock(required, taskExecutor);
    }

public:
    void set_required_quantity(lockcount_t required_) { required = required_; }
    bool needs_lock() const { return required != 0; }
    version_t finished(TaskExecutor<Options> &taskExecutor) {
        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        version_t ver;
        if (this_->use_contrib())
            ver = this_->handle->increase_current_version_no_unlock(taskExecutor);
        else
            ver = this_->handle->increase_current_version_unlock(required, taskExecutor);
        return ver;
    }
};

} // namespace detail

// ============================================================================
// Access
// ============================================================================
// Wraps up the selection of which action to perform depending on access type
template<typename Options>
class Access
  : public detail::Access_Lockable<Options>,
    public detail::Access_Contributions<Options>
{
public:
    typedef typename Options::AccessInfoType AccessInfo;
    typedef typename Options::version_t version_t;

    Handle<Options> *handle;
    version_t required_version;

    Access() {}
    Access(Handle<Options> *handle_, typename Options::version_t version_)
    : handle(handle_), required_version(version_) {}

    Handle<Options> *get_handle() const {
        return handle;
    }
};

} // namespace sg

#endif // SG_ACCESS_HPP_INCLUDED
