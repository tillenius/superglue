#include "sg/superglue.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {};

const int num = 10, n1 = 1000, n2 = 1000;
int data[num] = {0};
Handle<Options> h[num];

struct MyTask : public Task<Options> {
    int i;
    MyTask(size_t i_, Handle<Options> &h) : i(i_) {
        register_access(ReadWriteAdd::write, h);
    }
    void run() { ++data[i]; }
};

struct TaskCreator : public Task<Options> {
    SuperGlue<Options> &sg;
    size_t begin, end;
    TaskCreator(SuperGlue<Options> &sg_,
                size_t begin_, size_t end_)
    : sg(sg_), begin(begin_), end(end_) {}

    void run() {
        for (size_t i = begin; i < end; ++i) {
            size_t idx = i % num;
            sg.submit(new MyTask(idx, h[idx]));
        }
    }
};

int main() {
    SuperGlue<Options> sg;
    for (int i = 0; i < n1; ++i)
        sg.submit(new TaskCreator(sg, i*n2, (i+1)*n2));
    sg.barrier();

    int res = 0;
    for (int i = 0; i < num; ++i)
        res += data[i];
    std::cout << "result=" << res << std::endl;
    return 0;
}
