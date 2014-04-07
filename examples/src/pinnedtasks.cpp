#include "sg/superglue.hpp"
#include <iostream>

ThreadIDType id = 0;

struct Options : public DefaultOptions<Options> {
    typedef Disable Stealing;
    typedef Enable PauseExecution;
};

struct MyTask : public Task<Options> {
    void run() {
        if (id == 0)
            id = ThreadUtil::get_current_thread_id();
        if (id != ThreadUtil::get_current_thread_id()) {
            std::cerr << "Task running on wrong thread" << std::endl;
            exit(0);
        }
    }
};

int main() {

    SuperGlue<Options> sg;

    for (int i = 0; i < 1000; ++i)
        sg.submit(new MyTask(), 0); // all tasks added to readyqueue 0

    // No tasks will be executed yet

    sg.start_executing();

    // Wait for all tasks to finish
    sg.barrier();

    return 0;
}
