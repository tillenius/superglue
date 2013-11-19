#include "superglue.hpp"

#include <limits>

struct Options : public DefaultOptions<Options> {
    typedef Disable Lockable;
    typedef ReadWrite AccessInfoType;
};

int main() {
    ThreadManager<Options> tm;
    return 0;
}

