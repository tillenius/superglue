#include "superglue.hpp"

#include <limits>

struct Options : public DefaultOptions<Options> {
    typedef int version_t;
};

int main() {
    ThreadManager<Options> tm;
    return 0;
}

