#include "superglue.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {};

struct MyTask : public Task<Options> {
    void run() {
        std::cout << "Hello world!" << std::endl;
    }
};

int main() {
    ThreadManager<Options> tm;
    tm.submit(new MyTask());
    return 0;
}

