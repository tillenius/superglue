#include "platform/mutex.hpp"
#include <cassert>

int main() {
    Mutex m;
    {
        MutexLock ml(m);
    }
    {
        MutexLockReleasable ml(m);
        ml.release();
    }
    return 0;
}

