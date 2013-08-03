#include "superglue.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {
    // This option must be enabled in order to make task submission thread safe
    typedef Enable ThreadSafeSubmit;
};

const int num = 10, n1 = 1000, n2 = 1000;
int data[num] = {0};
Handle<Options> h[num];

struct MyTask : public Task<Options> {
    int i;
    MyTask(size_t i_, Handle<Options> *h) : i(i_) {
        registerAccess(ReadWriteAdd::write, h);
    }
    void run() { ++data[i]; }
};

struct TaskCreator : public Task<Options> {
    ThreadManager<Options> &tm;
    size_t begin, end;
    TaskCreator(ThreadManager<Options> &tm_,
                size_t begin_, size_t end_)
    : tm(tm_), begin(begin_), end(end_) {}

    void run() {
        for (size_t i = begin; i < end; ++i) {
            size_t idx = i % num;
            tm.submit(new MyTask(idx, &h[idx]));
        }
    }
};

int main() {
    ThreadManager<Options> tm;
    for (int i = 0; i < n1; ++i)
        tm.submit(new TaskCreator(tm, i*n2, (i+1)*n2));
    tm.barrier();

    int res = 0;
    for (int i = 0; i < num; ++i)
        res += data[i];
    std::cout << "result=" << res << std::endl;
    return 0;
}
