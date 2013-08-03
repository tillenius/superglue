#include "superglue.hpp"
#include <iostream>

ThreadIDType id = 0;

struct Options : public DefaultOptions<Options> {
    typedef Disable Stealing;
    typedef Enable PauseExecution;
};

struct MyTask : public Task<Options> {
    void run() {
        if (id == 0)
            id = ThreadUtil::getCurrentThreadId();
        if (id != ThreadUtil::getCurrentThreadId()) {
            std::cerr << "Task running on wrong thread" << std::endl;
            exit(0);
        }
    }
};

int main() {

    ThreadManager<Options> tm;

    for (int i = 0; i < 1000; ++i)
        tm.submit(new MyTask(), 0); // all tasks added to readyqueue 0

    // No tasks will be executed yet

    tm.setMayExecute(true); // allow tasks to be executed

    // Wait for all tasks to finish
    tm.barrier();

    return 0;
}
