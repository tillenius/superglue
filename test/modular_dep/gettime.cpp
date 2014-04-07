#include "sg/platform/gettime.hpp"
#include <cassert>

int main() {
    Time::TimeUnit t0 = Time::getTimeStart();
    Time::TimeUnit t1 = Time::getTime();
    Time::TimeUnit t2 = Time::getTimeStop();
    assert(t0 <= t1);
    assert(t1 <= t2);
    return 0;
}

