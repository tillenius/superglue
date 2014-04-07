#include "sg/platform/threads.hpp"
#include "sg/platform/threadutil.hpp"
#include "sg/platform/affinity.hpp"
#include <cassert>

using namespace sg;

bool ok = false;
bool ok2 = false;

struct thread : public Thread {
    virtual void run() {
        ok = true;
    }
};

int main() {
    assert(ThreadUtil::get_num_cpus() > 0);
    ThreadUtil::get_current_thread_id();

    thread thr;
    thr.start();
    thr.join();
    assert(ok);

    affinity_cpu_set cpu_set;
    cpu_set.set(0);
    ThreadAffinity::set_affinity(cpu_set);
    return 0;
}
