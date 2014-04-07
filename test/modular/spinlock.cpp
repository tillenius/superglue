#include "sg/core/spinlock.hpp"
#include <cassert>

using namespace sg;

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

    return 0;
}
