#include "platform/spinlock.hpp"
#include <cassert>

int main() {
    SpinLock sl;
    assert(!sl.is_locked());
    sl.lock();
    assert(sl.is_locked());
    assert(!sl.try_lock());
    sl.unlock();
    assert(!sl.is_locked());
    assert(sl.try_lock());
    assert(sl.is_locked());
    sl.unlock();
    assert(!sl.is_locked());

    {
        SpinLockScoped lock(sl);
        assert(sl.is_locked());
    }
    assert(!sl.is_locked());


    {
        SpinLockTryLock lock(sl);
        assert(sl.is_locked());
        {
            SpinLockTryLock lock2(sl);
            assert(sl.is_locked());
        }
        assert(sl.is_locked());
    }
    assert(!sl.is_locked());

    {
        SpinLockScopedReleasable lock(sl);
        assert(sl.is_locked());
        lock.release();
        assert(!sl.is_locked());
    }

    return 0;
}
