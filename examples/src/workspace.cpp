#include "superglue.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {
    typedef Enable PassTaskExecutor;
    typedef Enable ThreadWorkspace;
    enum { ThreadWorkspace_size = 102400 };
};

struct MyTask : public Task<Options> {
    int *a;

    MyTask(int *a_) : a(a_) {}

    // The run() method now takes a TaskExecutor<Options> * parameter.
    void run(TaskExecutor<Options> &te) {
        // Allocate some memory
        int *mem1 = (int *) te.getThreadWorkspace(1024 * sizeof(int));
        // Allocate some more memory
        int *mem2 = (int *) te.getThreadWorkspace(1024 * sizeof(int));

        // Fill the allocated memory
        for (int i = 0; i < 1024; ++i)
            mem2[i] = mem1[i] = i;

        // Sum the memory
        *a = 0.0;
        for (int i = 0; i < 1024; ++i)
            *a += mem2[i] + mem1[i];

        // Memory is automatically freed when task is finished
    }
};

int main() {
    // Shared array divided into slices
    int res;

    ThreadManager<Options> tm;
    tm.submit(new MyTask(&res));
    tm.barrier();

    // The data may be accessed here, after the barrier
    std::cout << "result=" << res << std::endl;
    return 0;
}
