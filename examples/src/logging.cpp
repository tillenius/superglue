#include "superglue.hpp"
#include "core/instr_tasktiming.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <cstddef>

using namespace std;

//
// This example activates logging and execute some dummy tasks.
//
// The application creates a files "output.log" which contains
// execution trace information such as which thread executes
// which task, at what time and for how long.
//
// There is a python script in scripts/ that can be used to
// draw the execution trace. It can be used like this:
//
//    ../../script/drawsched.py execution.log
//

//===========================================================================
// Task Library Options
//===========================================================================
struct Options : public DefaultOptions<Options> {
    typedef Enable TaskName;
    typedef Enable Logging;
    typedef TaskExecutorTiming<Options> TaskExecutorInstrumentation;
};

//===========================================================================
// Tasks
//===========================================================================
class TaskA : public Task<Options, 1> {
public:
    TaskA(Handle<Options> &h) {
        registerAccess(ReadWriteAdd::write, &h);
    }

    void run() {
        Time::TimeUnit t = Time::getTime();
        while (Time::getTime() < t + 1000000);
    }

    std::string getName() { return "A"; }
};

class TaskB : public Task<Options, 2> {
private:
    size_t delay;

public:
    TaskB(Handle<Options> &h, Handle<Options> &h1, size_t delay_)
      : delay(delay_) {
        registerAccess(ReadWriteAdd::read, &h);
        registerAccess(ReadWriteAdd::write, &h1);
    }

    void run() {
        Time::TimeUnit t = Time::getTime();
        while (Time::getTime() < t + delay);
    }
    std::string getName() { return "B"; }
};

class TaskD : public Task<Options, 1> {
public:
    TaskD(Handle<Options> &h) {
        registerAccess(ReadWriteAdd::read, &h);
    }

    void run() {
        Time::TimeUnit t = Time::getTime();
        while (Time::getTime() < t + 1000000);
    }
    std::string getName() { return "D"; }
};

class TaskE : public Task<Options, 2> {
public:
    TaskE(Handle<Options> &h1, Handle<Options> &h2) {
        registerAccess(ReadWriteAdd::read, &h1);
        registerAccess(ReadWriteAdd::read, &h2);
    }

    void run() {
        Time::TimeUnit t = Time::getTime();
        while (Time::getTime() < t + 1000000);
    }
    std::string getName() { return "E"; }
};

//===========================================================================
// main
//===========================================================================
int main(int argc, char *argv[]) {

    int num_threads = -1;
    if (argc == 2) {
        num_threads = (size_t) atoi(argv[1]);
    }
    else if (argc != 1) {
        printf("usage: %s [num_cores]\n", argv[0]);
        exit(0);
    }

    ThreadManager<Options> tm(num_threads);
    Handle<Options> a, b, c;
    tm.submit(new TaskA(a));
    tm.submit(new TaskB(a, b, 1000000));
    tm.submit(new TaskB(a, c, 2000000));
    tm.submit(new TaskD(b));
    tm.submit(new TaskE(b, c));
    tm.barrier();

    Log<Options>::dump("execution.log");
    return 0;
}
