#include "sg/platform/perfcount.hpp"

#include <cassert>

int main() {

    PerformanceCounter perf_cycles(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
    PerformanceCounter perf_instr(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

    double d = 1.0;
    const size_t num_iterations = 100;

    perf_cycles.start();
    perf_instr.start();
    unsigned long long start_cycles = perf_cycles.readCounter();
    unsigned long long start_instr = perf_instr.readCounter();
    for (size_t i = 0; i < num_iterations; ++i)
        d += 1e-5;
    perf_cycles.stop();
    perf_instr.stop();

    unsigned long long stop_cycles = perf_cycles.readCounter();
    unsigned long long stop_instr = perf_instr.readCounter();

    assert(stop_cycles > start_cycles);
    assert(stop_instr > start_instr);
    assert((stop_instr - start_instr) < (stop_cycles - start_cycles));
    assert((stop_instr - start_instr) > num_iterations);

  return 0;
}
