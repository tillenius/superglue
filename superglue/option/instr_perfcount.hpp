#include "platform/perfcount.hpp"

template<typename Options>
struct PerfTiming {
    Time::TimeUnit start, stop;
    unsigned long long cpu_clock;
    unsigned long long task_clock;
    unsigned long long context_switches;
    PerformanceCounter *perf[3];

    PerfTiming() {
        perf[0] = new PerformanceCounter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK);
        perf[1] = new PerformanceCounter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK);
        perf[2] = new PerformanceCounter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES);
    }
    ~PerfTiming() {
        delete perf[0];
        delete perf[1];
        delete perf[2];
    }
    void init(TaskExecutor<Options> &te) {}
    void destroy(TaskExecutor<Options> &te) {}
    void runTaskBefore(TaskBase<Options> *) {
        cpu_clock = perf[0]->readCounter();
        task_clock = perf[1]->readCounter();
        context_switches = perf[2]->readCounter();
        perf[0]->start();
        perf[1]->start();
        perf[2]->start();
        start = Time::getTime();
    }
    void runTaskAfter(TaskBase<Options> *task) {
        stop = Time::getTime();
        perf[2]->stop();
        perf[1]->stop();
        perf[0]->stop();
        unsigned long long end_cpu_clock = perf[0]->readCounter();
        unsigned long long end_task_clock = perf[1]->readCounter();
        unsigned long long end_context_switches = perf[2]->readCounter();
        char txt[80];
        sprintf(txt, "%s cpu=%llu task=%llu cs=%llu", 
            detail::GetName::getName(task).c_str(),
            end_cpu_clock - cpu_clock,
            end_task_clock - task_clock,
            end_context_switches - context_switches);
        Log<Options>::addEvent(txt, start, stop);
    }
    static void finalize() {}
    static void taskNotRunDeps() {}
    static void taskNotRunLock() {}
};
