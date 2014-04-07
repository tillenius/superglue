#include "sg/superglue.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {};

struct MyTask : public Task<Options> {
    void run() {
        std::cout << "Hello world!" << std::endl;
    }
};

int main() {
    SuperGlue<Options> sg;
    sg.submit(new MyTask());
    return 0;
}

