#include "sg/superglue.hpp"

#include <limits>

struct Options : public DefaultOptions<Options> {
    typedef Disable Lockable;
};

int main() {
    SuperGlue<Options> sg;
    return 0;
}

