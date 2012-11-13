#ifndef __PERFCOUNT_HPP__
#define __PERFCOUNT_HPP__

// http://lxr.free-electrons.com/source/tools/perf/design.txt

// PERF_TYPE_SOFTWARE:
//     PERF_COUNT_SW_CPU_CLOCK         = 0,
//     PERF_COUNT_SW_TASK_CLOCK        = 1,
//     PERF_COUNT_SW_PAGE_FAULTS       = 2,
//     PERF_COUNT_SW_CONTEXT_SWITCHES  = 3,
//     PERF_COUNT_SW_CPU_MIGRATIONS    = 4,
//     PERF_COUNT_SW_PAGE_FAULTS_MIN   = 5,
//     PERF_COUNT_SW_PAGE_FAULTS_MAJ   = 6,
//     PERF_COUNT_SW_ALIGNMENT_FAULTS  = 7,
//     PERF_COUNT_SW_EMULATION_FAULTS  = 8,

// PERF_TYPE_HARDWARE:
//     PERF_COUNT_HW_CPU_CYCLES                = 0,
//     PERF_COUNT_HW_INSTRUCTIONS              = 1,
//     PERF_COUNT_HW_CACHE_REFERENCES          = 2,
//     PERF_COUNT_HW_CACHE_MISSES              = 3,
//     PERF_COUNT_HW_BRANCH_INSTRUCTIONS       = 4,
//     PERF_COUNT_HW_BRANCH_MISSES             = 5,
//     PERF_COUNT_HW_BUS_CYCLES                = 6,

// PERF_TYPE_HW_CACHE:
//    PERF_COUNT_HW_CACHE_L1D         = 0,
//    PERF_COUNT_HW_CACHE_L1I         = 1,
//    PERF_COUNT_HW_CACHE_LL          = 2,
//    PERF_COUNT_HW_CACHE_DTLB        = 3,
//    PERF_COUNT_HW_CACHE_ITLB        = 4,
//    PERF_COUNT_HW_CACHE_BPU         = 5,
//
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_BPU | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
// { .type = PERF_TYPE_HW_CACHE, .config = (PERF_COUNT_HW_CACHE_BPU | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},

#include <sys/ioctl.h>
#include <linux/perf_event.h>
extern "C" {
#include <sys/syscall.h>
}
#include <iostream>

class PerformanceCounter {
private:
    int fd;

    // pid == 0: the counter is attached to the current task.
    // pid > 0: the counter is attached to a specific task (if the current task has sufficient privilege to do so)
    // pid < 0: all tasks are counted (per cpu counters)

    // cpu >= 0: the counter is restricted to a specific CPU
    // cpu == -1: the counter counts on all CPUs

    // (Note: the combination of 'pid == -1' and 'cpu == -1' is not valid.)
    // A 'pid > 0' and 'cpu == -1' counter is a per task counter that counts
    // events of that task and 'follows' that task to whatever CPU the task
    // gets schedule to. Per task counters can be created by any user, for
    // their own tasks.

    // A 'pid == -1' and 'cpu == x' counter is a per CPU counter that counts
    // all events on CPU-x. Per CPU counters need CAP_SYS_ADMIN privilege.

    // The 'flags' parameter is currently unused and must be zero.

    // group_fd = -1: A single counter on its own is created and is
    //                considered to be a group with only 1 member.)
    static int
    sys_perf_event_open(struct perf_event_attr *attr,
            pid_t pid, int cpu, int group_fd,
            unsigned long flags)
    {
        return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
    }

public:
    PerformanceCounter() : fd(-1) {}
    PerformanceCounter(unsigned int type, unsigned long long config) {
        init(type, config);
    }

    ~PerformanceCounter() {
        if (fd >= 0)
            close(fd);
    }

    void init(unsigned int type, unsigned long long config) {
        struct perf_event_attr attr = {0};
        attr.type = type;
        attr.config = config;
        attr.inherit = 1;
        attr.disabled = 1;
        attr.size = sizeof(struct perf_event_attr);
        fd = sys_perf_event_open(&attr, 0, -1, -1, 0);
        if (fd < 0) {
            std::cerr << "sys_perf_event_open failed " << fd << std::endl;
            exit(1);
        }
    }

    void start() {
        ioctl(fd, PERF_EVENT_IOC_ENABLE);
    }
    void stop() {
        ioctl(fd, PERF_EVENT_IOC_DISABLE);
    }

    unsigned long long readCounter() {
        unsigned long long count;
        size_t res = read(fd, &count, sizeof(unsigned long long));
        if (res != sizeof(unsigned long long)) {
            std::cerr << "read() failed " << res << std::endl;
            exit(1);
        }
        return count;
    }

};

// Example usage:
//
//int main() {
//
//    Perf perf(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
//    Perf perf2(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
//
//    double d = 1.0;
//
//    perf.start();
//    for (int i = 0; i < 1000000; ++i)
//        d += 1e-5;
//
//    perf2.start();
//    for (int i = 0; i < 1000000; ++i)
//        d += 1e-5;
//    perf.stop();
//    perf2.stop();
//
//    unsigned long long cycles = perf.readCounter();
//    unsigned long long cachemiss = perf2.readCounter();
//
//    printf("cycles = %lld\n", cycles);
//    printf("cache misses = %lld\n", cachemiss);
//
//  return 0;
//}

#endif // __PERFCOUNT_HPP__
