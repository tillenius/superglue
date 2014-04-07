#include "sg/platform/atomic.hpp"
#include <cassert>

using namespace sg;

int main() { 

    // atomic increase and decrease

    unsigned int a = 0;
    Atomic::increase(&a);
    assert(a == 1);
    Atomic::decrease(&a);
    assert(a == 0);
    assert(Atomic::add_nv(&a, 2u) == 2);
    assert(Atomic::increase_nv(&a) == 3);
    assert(Atomic::decrease_nv(&a) == 2);

    // compare and swap

    assert(Atomic::cas(&a, 0u, 4u) == 2);
    assert(Atomic::cas(&a, 2u, 3u) == 2);
    assert(a == 3);
    assert(Atomic::swap(&a, 5u) == 3);
    assert(a == 5);

    // lock primitives

    unsigned int lock = 1;
    assert(!Atomic::lock_test_and_set(&lock));
    Atomic::lock_release(&lock);
    assert(lock == 0);
    assert(Atomic::lock_test_and_set(&lock));
    assert(lock == 1);

    // barriers etc

    Atomic::memory_fence_enter();
    Atomic::memory_fence_exit();
    Atomic::memory_fence_producer();
    Atomic::memory_fence_consumer();
    Atomic::yield();
    Atomic::rep_nop();
    Atomic::compiler_fence();
    return 0; 
}

