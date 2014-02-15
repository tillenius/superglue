#ifndef __BARRIER_HPP__
#define __BARRIER_HPP__

#include "platform/atomic.hpp"

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
            Atomic::rep_nop();
    }
};

#endif // __BARRIER_HPP__
