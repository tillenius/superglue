#include "platform/threads.hpp"
#include <cassert>

bool ok = false;
bool ok2 = false;

struct thread : public Thread {
    virtual void run() {
        ThreadUtil::sleep(100);
        ok = true;
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

    ThreadUtil::setAffinity(0);
    return 0;
}
