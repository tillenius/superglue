#ifndef __ACCESS_HPP__
#define __ACCESS_HPP__

#include <algorithm>
#include <cstring>

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
    static bool useContrib() { return false; }
    static void setUseContrib(bool ) {}
};

template<typename Options>
class Access_Contributions<Options, typename Options::Enable> {
private:
    bool useContribFlag;
public:
    typedef typename Options::ContributionType Contribution;

    Access_Contributions() : useContribFlag(false) {}
    void setUseContrib(bool value) { useContribFlag = value; }
    bool useContrib() const { return useContribFlag; }
    void addContribution(Contribution c) {
        this->getHandle()->addContribution(c);
    }
};

// ============================================================================
// Option Lockable
// ============================================================================
template<typename Options, typename T = typename Options::Lockable> class Access_Lockable;

template<typename Options>
class Access_Lockable<Options, typename Options::Disable> {
    typedef typename Options::version_t version_t;
public:
    static bool getLock() { return true; }
    static bool needsLock() { return false; }
    static void releaseLock(TaskExecutor<Options> &) {}
    static bool getLockOrNotify(TaskBase<Options> *) { return true; }
    static void setNeedsLock(bool) {}
    version_t finished(TaskExecutor<Options> &taskExecutor) {
        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        return this_->handle->increaseCurrentVersion(taskExecutor);
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
    bool getLockOrNotify(TaskBase<Options> *task) const {
        if (required == 0)
            return true;

        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        return this_->handle->getLockOrNotify(required, task);
    }

    // Get lock if its free, or return false.
    // Low level interface to lock several objects simultaneously
    bool getLock() const {
        if (required == 0)
            return true;

        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        return this_->handle->getLock(required);
    }

    // Low level interface to unlock when failed to lock several objects
    void releaseLock(TaskExecutor<Options> &taskExecutor) const {
        if (required == 0)
            return;

        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        this_->handle->releaseLock(required, taskExecutor);
    }

public:
    void setNeedsLock(bool needsLock_) { required = needsLock_ ? 1 : 0; }
    bool needsLock() const { return required != 0; }
    version_t finished(TaskExecutor<Options> &taskExecutor) {
        const Access<Options> *this_(static_cast<const Access<Options> *>(this));
        version_t ver;
        if (this_->useContrib())
            ver = this_->handle->increaseCurrentVersionNoUnlock(taskExecutor);
        else
            ver = this_->handle->increaseCurrentVersionUnlock(required, taskExecutor);
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

    // The handle to the object
    Handle<Options> *handle;
    // Required version
    version_t requiredVersion;

    // Constructors
    Access() {}
    Access(Handle<Options> *handle_, typename Options::version_t version_)
    : handle(handle_), requiredVersion(version_) {}

    Handle<Options> *getHandle() const {
        return handle;
    }
};

#endif // __ACCESS_HPP__
