#include "platform/barrier.hpp"
#include <cassert>

int main() {
    Barrier b(1);
    b.wait();
    return 0;
}

