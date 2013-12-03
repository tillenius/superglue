#ifndef __SPINLOCK_HPP__
#define __SPINLOCK_HPP__

#include <pthread.h>
#include "platform/atomic.hpp"

class SpinLockAtomic {
public:
    volatile unsigned int v_;
    char padding[64-sizeof(volatile unsigned int)];

    SpinLockAtomic(const SpinLockAtomic &);
    const SpinLockAtomic &operator=(const SpinLockAtomic &);
public:
    SpinLockAtomic() : v_(0) {}

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
        while (v_ == 1);
        while (!try_lock())
            while (v_ == 1);
    }
};

#ifndef __APPLE__
class SpinLockPthreads {
private:
    pthread_spinlock_t spinlock;

    SpinLockPthreads(const SpinLockPthreads &);
    const SpinLockPthreads &operator=(const SpinLockPthreads &);
public:
    SpinLockPthreads() {
        pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
    }
    ~SpinLockPthreads() {
        pthread_spin_destroy(&spinlock);
    }

    bool try_lock() {
        return pthread_spin_trylock(&spinlock) == 0;
    }
    void unlock() {
        pthread_spin_unlock(&spinlock);
    }
    void lock() {
        pthread_spin_lock(&spinlock);
    }
};
#endif // __APPLE__

typedef SpinLockAtomic SpinLock;

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

    SpinLockTryLock( SpinLockTryLock const & );
    SpinLockTryLock & operator=( SpinLockTryLock const & );

public:
    bool success;

    explicit SpinLockTryLock( SpinLock & sp ): sp_( sp ) {
        success = sp.try_lock();
    }

    ~SpinLockTryLock() {
        if (success)
            sp_.unlock();
    }
};

class SpinLockScopedReleasable {
private:
    SpinLock & sp_;
    bool released;

    SpinLockScopedReleasable( SpinLockScopedReleasable const & );
    SpinLockScopedReleasable & operator=( SpinLockScopedReleasable const & );

public:
    explicit SpinLockScopedReleasable( SpinLock & sp ): sp_( sp ), released(false) {
        sp.lock();
    }

    void release() {
        sp_.unlock();
        released = true;
    }

    ~SpinLockScopedReleasable() {
        if (!released)
            sp_.unlock();
    }
};

#endif // __SPINLOCK_HPP__
