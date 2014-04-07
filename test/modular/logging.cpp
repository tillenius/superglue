#include "sg/superglue.hpp"
#include "sg/option/log.hpp"

struct Options : public DefaultOptions<Options> {};

int main() {
    Log<Options>::register_thread(0);
    Log<Options>::log("Test", 0, 1);
    return 0;
}
