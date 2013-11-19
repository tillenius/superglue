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

    affinity_cpu_set cpu_set;
    cpu_set.set(0);
    ThreadUtil::setAffinity(cpu_set);
    return 0;
}
