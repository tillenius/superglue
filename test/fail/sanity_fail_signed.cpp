#include "sg/superglue.hpp"

#include <limits>

struct Options : public DefaultOptions<Options> {
    typedef int version_t;
};

int main() {
    SuperGlue<Options> sg;
    return 0;
}

