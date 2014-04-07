#include "sg/platform/gettime.hpp"
#include <cassert>

int main() {
    Time::TimeUnit t0 = Time::getTime();
    Time::TimeUnit t1 = Time::getTime();
    assert(t0 <= t1);
    return 0;
}

