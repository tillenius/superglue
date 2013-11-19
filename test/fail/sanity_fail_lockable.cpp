#include "superglue.hpp"

#include <limits>

struct Options : public DefaultOptions<Options> {
    typedef Disable Lockable;
};

int main() {
    ThreadManager<Options> tm;
    return 0;
}

