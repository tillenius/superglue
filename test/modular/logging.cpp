#include "superglue.hpp"

struct Options : public DefaultOptions<Options> {
    typedef Enable Logging;
};

int main() {
    Log<Options>::registerThread(0);
    Log<Options>::log("Test", 0, 1);
    return 0;
}
