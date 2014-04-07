#include "sg/superglue.hpp"

#include "sg/option/access_readwrite.hpp"

struct Options : public DefaultOptions<Options> {
    typedef Disable Lockable;
    typedef ReadWrite AccessInfoType;
};

struct MyTask : Task<Options> {
    Handle<Options> &h;
    MyTask(Handle<Options> &h_) : h(h_) {
        register_access(ReadWrite::read, h);
    }
    void run() {}
};

int main() {
    SuperGlue<Options> sg;
    Handle<Options> h;
    MyTask t(h);
    return 0;
}
