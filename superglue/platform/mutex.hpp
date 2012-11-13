#ifndef __MUTEX_HPP__
#define __MUTEX_HPP__

// Defines:
//   Mutex
//   MutexLock
//   MutexLockReleasable
//   MutexTryLock
//   Signal
//   SignalLock
//   SignalTryLock
//   Barrier

#include <iostream>
#include <sstream>
#include <cassert>

class Signal;

#ifndef _MSC_VER
#define PTHREADS
#endif

#ifdef PTHREADS
#include <pthread.h>

class MutexLock;

class Mutex {
    friend class MutexLock;
    friend class MutexLockReleasable;
    friend class MutexTryLock;
private:
    Mutex(const Mutex &);
    const Mutex &operator=(const Mutex &);

protected:
    pthread_mutex_t mutex;
public:
    Mutex() {
        pthread_mutex_init(&mutex, NULL);
    }
    ~Mutex() {
        pthread_mutex_destroy(&mutex);
    }
};

// Scoped lock
class MutexLock {
private:
    Mutex &mutex;

    MutexLock(const MutexLock &);
    const MutexLock &operator=(const MutexLock &);
public:
    MutexLock(Mutex &mutex_) : mutex(mutex_) {
        pthread_mutex_lock(&mutex.mutex);
    }
    ~MutexLock() {
        pthread_mutex_unlock(&mutex.mutex);
    }
};

class MutexLockReleasable {
private:
    Mutex &mutex;
    bool released;

    MutexLockReleasable(const MutexLockReleasable &);
    const MutexLockReleasable &operator=(const MutexLockReleasable &);

public:
    MutexLockReleasable(Mutex &mutex_) : mutex(mutex_), released(false) {
        pthread_mutex_lock(&mutex.mutex);
    }
    ~MutexLockReleasable() {
        if (!released)
            pthread_mutex_unlock(&mutex.mutex);
    }
    void release() {
        released = true;
        pthread_mutex_unlock(&mutex.mutex);
    }
};

// Scoped lock that never blocks
// Check if the mutex was obtained using isLocked()
class MutexTryLock {
private:
    Mutex &mutex;
    bool success;

    MutexTryLock(const MutexTryLock &);
    const MutexTryLock &operator=(const MutexTryLock &);
public:
    MutexTryLock(Mutex &mutex_) : mutex(mutex_) {
        int ret = pthread_mutex_trylock(&mutex.mutex);
        success = (ret == 0);
    }
    ~MutexTryLock() {
        if (success)
            pthread_mutex_unlock(&mutex.mutex);
    }
    bool isLocked() {
        return success;
    }
};


class Signal {
    friend class SignalLock;
    friend class SignalTryLock;
private:
    Signal(const Signal &);
    const Signal &operator=(const Signal &);
protected:
    pthread_mutex_t mutex;
    pthread_cond_t cond;
public:
    Signal() {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
    }
    ~Signal() {
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mutex);
    }
    void broadcast() {
        pthread_cond_broadcast(&cond);
    }
    void signal() {
        pthread_cond_signal(&cond);
    }
};

class SignalLock {
private:
    Signal &sign;
    bool released;

    SignalLock(const SignalLock &);
    const SignalLock &operator=(const SignalLock &);
public:
    explicit SignalLock(Signal &sign_) : sign(sign_), released(false) {
        lock();
    }
    explicit SignalLock(Signal &sign_, bool) : sign(sign_), released(true) {
    }
    void lock() {
        pthread_mutex_lock(&sign.mutex);
        released = false;
    }
    ~SignalLock() {
        if (!released)
            pthread_mutex_unlock(&sign.mutex);
    }
    void release() {
        released = true;
        pthread_mutex_unlock(&sign.mutex);
    }
    void wait() {
        // "Spurious wakeups from the pthread_cond_wait() function may occur."
        pthread_cond_wait(&sign.cond, &sign.mutex);
    }
    void broadcast() {
        pthread_cond_broadcast(&sign.cond);
    }
    void signal() {
        pthread_cond_signal(&sign.cond);
    }
};

class SignalTryLock {
private:
    Signal &sign;
    bool success;

    SignalTryLock(const SignalTryLock &);
    const SignalTryLock &operator=(const SignalTryLock &);
public:
    explicit SignalTryLock(Signal &sign_) : sign(sign_) {
        int res = pthread_mutex_trylock(&sign.mutex);
        success = res == 0;
    }
    ~SignalTryLock() {
        if (success)
            pthread_mutex_unlock(&sign.mutex);
    }
    void release() {
        success = false;
        pthread_mutex_unlock(&sign.mutex);
    }
    void wait() {
        // "Spurious wakeups from the pthread_cond_wait() function may occur."
        pthread_cond_wait(&sign.cond, &sign.mutex);
    }
    void broadcast() {
        pthread_cond_broadcast(&sign.cond);
    }
    void signal() {
        pthread_cond_signal(&sign.cond);
    }
    bool isLocked() {
        return success;
    }
};

//class Barrier {
//    Barrier(const Barrier &);
//    const Barrier &operator=(const Barrier &);
//private:
//    pthread_barrier_t barrier;
//
//public:
//    Barrier(size_t numThreads) {
//        assert(pthread_barrier_init(&barrier, NULL, numThreads) == 0);
//    }
//    ~Barrier() {
//        pthread_barrier_destroy(&barrier);
//    }
//    // spurious wake-ups from Barrier.wait are NOT accepted!
//    void wait() {
//        int res = pthread_barrier_wait(&barrier);
//        assert(res == PTHREAD_BARRIER_SERIAL_THREAD || res == 0);
//    }
//};

class Barrier {
    Barrier(const Barrier &);
    const Barrier &operator=(const Barrier &);
private:
    size_t counter;

public:
    Barrier(size_t numThreads) {
        counter = numThreads;
    }
    // Spurious wake-ups from Barrier.wait are NOT accepted
    void wait() {
        Atomic::decrease(&counter);
        while (counter > 0)
            Atomic::yield();
    }
};

#else

#define NOMINMAX
#include <windows.h>

class MutexLock;

class Mutex {
    friend class MutexLock;
    friend class MutexLockReleasable;
    friend class MutexTryLock;
private:
    Mutex(const Mutex &);
    const Mutex &operator=(const Mutex &);
protected:
    CRITICAL_SECTION mutex;
public:
    Mutex() {
        if (!InitializeCriticalSectionAndSpinCount(&mutex, 0x80000400) )
            MessageBoxA(0, "Failed to create mutex", 0, MB_OK);
    }
    ~Mutex() {
        DeleteCriticalSection(&mutex);
    }
};

class MutexLock {
private:
    Mutex &mutex;

    MutexLock(const MutexLock &);
    const MutexLock &operator=(const MutexLock &);
public:
    MutexLock(Mutex &mutex_) : mutex(mutex_) {
        EnterCriticalSection(&mutex.mutex);
    }
    ~MutexLock() {
        LeaveCriticalSection(&mutex.mutex);
    }
};

class MutexLockReleasable {
private:
    Mutex &mutex;
    bool released;

    MutexLockReleasable(const MutexLockReleasable &);
    const MutexLockReleasable &operator=(const MutexLockReleasable &);

public:
    MutexLockReleasable(Mutex &mutex_) : mutex(mutex_), released(false) {
        EnterCriticalSection(&mutex.mutex);
    }
    ~MutexLockReleasable() {
        if (!released)
            release();
    }
    void release() {
        released = true;
        LeaveCriticalSection(&mutex.mutex);
    }
};

class MutexTryLock {
private:
    Mutex &mutex;
    bool released;
    bool success;


    MutexTryLock(const MutexTryLock &);
    const MutexTryLock &operator=(const MutexTryLock &);

    void lock() {
        released = false;
        success = TryEnterCriticalSection(&mutex.mutex) != 0;
    }
public:
    MutexTryLock(Mutex &mutex_) : mutex(mutex_), released(false) {
        lock();
    }
    ~MutexTryLock() {
        if (isLocked())
            release();
    }
    void release() {
        LeaveCriticalSection(&mutex.mutex);
        released = true;
    }
    bool isLocked() {
        return (success && !released);
    }
};

class Signal {
    friend class SignalLock;
    friend class SignalTryLock;
private:
    CONDITION_VARIABLE signalHandle;
    CRITICAL_SECTION mutex;

    Signal(const Signal &);
    const Signal &operator=(const Signal &);

public:
    Signal() {
        if (!InitializeCriticalSectionAndSpinCount(&mutex, 0x80000400) )
            MessageBoxA(0, "Failed to create mutex", 0, MB_OK);
        InitializeConditionVariable(&signalHandle);
    }
    ~Signal() {
        DeleteCriticalSection(&mutex);
    }
    void signal() {
        WakeConditionVariable(&signalHandle);
    }
    void broadcast() {
        WakeAllConditionVariable(&signalHandle);
    }
};

class SignalLock {
private:
    Signal &sign;
    bool released;

    SignalLock(const SignalLock &);
    const SignalLock &operator=(const SignalLock &);

public:
    explicit SignalLock(Signal &sign_)  : sign(sign_) {
        lock();
    }

    ~SignalLock() {
        if (!released)
            release();
    }

    void lock() {
        released = false;
        EnterCriticalSection(&sign.mutex);
    }

    void release() {
        LeaveCriticalSection(&sign.mutex);
        released = true;
    }

    void signal() {
        WakeConditionVariable(&sign.signalHandle);
    }
    void broadcast() {
        WakeAllConditionVariable(&sign.signalHandle);
    }
    void wait() {
        // "Condition variables are subject to spurious wakeups (those not associated
        // with an explicit wake) and stolen wakeups (another thread manages to run
        // before the woken thread)."

        if (!SleepConditionVariableCS(&sign.signalHandle, &sign.mutex, INFINITE))
            MessageBoxA(0, "SignalLock failed to wait for signal", 0, MB_OK);
    }
};

class SignalTryLock {
private:
    Signal &sign;
    bool success;
    bool released;

    SignalTryLock(const SignalTryLock &);
    const SignalTryLock &operator=(const SignalTryLock &);

    void lock() {
        released = false;
        success = TryEnterCriticalSection(&sign.mutex) != 0;
    }

public:
    explicit SignalTryLock(Signal &sign_) : sign(sign_), released(false) {
        lock();
    }
    ~SignalTryLock() {
        if (isLocked())
            release();
    }
    void release() {
        WakeAllConditionVariable(&sign.signalHandle);
        LeaveCriticalSection(&sign.mutex);
        released = true;
    }
    void signal() {
        WakeConditionVariable(&sign.signalHandle);
    }
    void broadcast() {
        WakeAllConditionVariable(&sign.signalHandle);
    }
    void wait() {
        // "Condition variables are subject to spurious wakeups (those not associated
        // with an explicit wake) and stolen wakeups (another thread manages to run
        // before the woken thread)."

        if (!SleepConditionVariableCS(&sign.signalHandle, &sign.mutex, INFINITE))
            MessageBoxA(0, "SignalTryLock failed to wait for signal", 0, MB_OK);
    }
    bool isLocked() {
        return (success && !released);
    }
};

class Barrier {
private:
    Signal signal;
    volatile int counter;

public:
    Barrier(size_t numThreads) {
        counter = numThreads;
    }
    ~Barrier() {}

    // Spurious wake-ups from Barrier.wait are NOT accepted
    void wait() {
        SignalLock lock(signal);
        if (--counter == 0) {
            lock.broadcast();
            return;
        }
        while (counter > 0)
            lock.wait();
    }
};

#endif // PTHREADS

#endif // __MUTEX_HPP__
