
#include "superglue.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {};

ThreadManager<Options> tm;

struct MyTask : public Task<Options> {

    int begin, end;

    MyTask(int begin_, int end_)
    : begin(begin_), end(end_)
    {}

    void run() {
        if (end-begin >= 2) {
            int mid = begin+(end-begin+1)/2;
            if (mid - begin > 0) {
                tm.submit(new MyTask(begin, mid));
            }
            if (end - mid > 0) {
                tm.submit(new MyTask(mid, end));
            }
        }
        else {
            std::stringstream ss;
            ss << "[" << begin << "-" << end << "]" << std::endl;
            std::cerr << ss.str();
        }
    }
};

int main() {
    tm.submit(new MyTask(0, 9));
    tm.barrier();
    return 0;
}
