#ifndef SG_SPINLOCK_HPP_INCLUDED
#define SG_SPINLOCK_HPP_INCLUDED

#include "sg/platform/atomic.hpp"

namespace sg {

class SpinLock {
private:
    enum { CACHE_LINE_SIZE = 64 };
    unsigned int v_;
    char padding[CACHE_LINE_SIZE-sizeof(unsigned int)];

    SpinLock(const SpinLock &);
    const SpinLock &operator=(const SpinLock &);

public:
    SpinLock() : v_(0) {}

    bool try_lock() {
        return Atomic::lock_test_and_set(&v_);
    }
    bool is_locked() {
        return v_ != 0;
    }
    void unlock() {
        Atomic::lock_release(&v_);
    }
    void lock() {
        while (v_ == 1)
            Atomic::rep_nop();
        while (!try_lock())
            while (v_ == 1)
                Atomic::rep_nop();
    }
};

class SpinLockScoped {
private:
    SpinLock & sp_;

    SpinLockScoped( SpinLockScoped const & );
    SpinLockScoped & operator=( SpinLockScoped const & );
public:
    explicit SpinLockScoped( SpinLock & sp ): sp_( sp ) {
        sp.lock();
    }

    __attribute__((always_inline)) ~SpinLockScoped()  {
        sp_.unlock();
    }
};

class SpinLockTryLock {
private:
    SpinLock & sp_;

    SpinLockTryLock(SpinLockTryLock const &);
    SpinLockTryLock &operator=(SpinLockTryLock const &);

public:
    const bool success;

    explicit SpinLockTryLock(SpinLock &sp) : sp_(sp), success(sp.try_lock()) {}

    ~SpinLockTryLock() {
        if (success)
            sp_.unlock();
    }
};

} // namespace sg

#endif // SG_SPINLOCK_HPP_INCLUDED
