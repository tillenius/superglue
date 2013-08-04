#include "platform/threads.hpp"
#include <cassert>

bool ok = false;
bool ok2 = false;

struct thread : public FreeThread {
    virtual void run() {
        ThreadUtil::sleep(100);
        ok = true;
    }
};

struct pinned_thread : public Thread {
    virtual void run() {
        ThreadUtil::sleep(100);
        ok2 = true;
    }
};


int main() {
    assert(ThreadUtil::getNumCPUs() > 0);
    ThreadUtil::sleep(0);
    ThreadUtil::getCurrentThreadId();

    thread thr;
    thr.start();
    thr.join();
    assert(ok);

    {
        pinned_thread thr;
        thr.start(0);
        thr.getThreadId();
        thr.join();
        assert(ok2);
    }

    ThreadUtil::setAffinity(0);
    return 0;
}

