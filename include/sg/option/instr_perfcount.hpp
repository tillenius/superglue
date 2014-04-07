#ifndef SG_INSTR_PERFCOUNT_HPP_INCLUDED
#define SG_INSTR_PERFCOUNT_HPP_INCLUDED

// [[TODO]] MISSING TEST

#include "sg/platform/perfcount.hpp"

#include "sg/option/log.hpp"

namespace sg {

template<typename Options>
struct PerfTiming {
    Time::TimeUnit start, stop;
    unsigned long long cpu_clock;
    unsigned long long task_clock;
    unsigned long long context_switches;
    PerformanceCounter *perf[3];

    PerfTiming(int) {
        perf[0] = new PerformanceCounter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK);
        perf[1] = new PerformanceCounter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK);
        perf[2] = new PerformanceCounter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES);
    }
    ~PerfTiming() {
        delete perf[0];
        delete perf[1];
        delete perf[2];
    }
    void run_task_before(TaskBase<Options> *) {
        cpu_clock = perf[0]->read_counter();
        task_clock = perf[1]->read_counter();
        context_switches = perf[2]->read_counter();
        perf[0]->start();
        perf[1]->start();
        perf[2]->start();
        start = Time::getTime();
    }
    void run_task_after(TaskBase<Options> *task) {
        stop = Time::getTime();
        perf[2]->stop();
        perf[1]->stop();
        perf[0]->stop();
        unsigned long long end_cpu_clock = perf[0]->read_counter();
        unsigned long long end_task_clock = perf[1]->read_counter();
        unsigned long long end_context_switches = perf[2]->read_counter();
        char txt[80];
        sprintf(txt, "%s cpu=%llu task=%llu cs=%llu", 
            detail::GetName::get_name(task).c_str(),
            end_cpu_clock - cpu_clock,
            end_task_clock - task_clock,
            end_context_switches - context_switches);
        Log<Options>::log(txt, start, stop);
    }
};

} // namespace sg

#endif // SG_INSTR_PERFCOUNT_HPP_INCLUDED
