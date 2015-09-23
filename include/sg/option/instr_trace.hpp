#ifndef SG_INSTR_TRACE_HPP_INCLUDED
#define SG_INSTR_TRACE_HPP_INCLUDED

#include "sg/option/log.hpp"

namespace sg {

template<typename Options>
struct Trace {
    Time::TimeUnit start;
	Time::TimeUnit start_idle_time;
	Trace(int threadid) {
        Log<Options>::register_thread(threadid);
		start = start_idle_time = Time::getTime();
    }
    void run_task_before(TaskBase<Options> *) {
        const bool first = (start == start_idle_time);
        start = Time::getTime();
        if (!first)
            Log<Options>::add_idle_time(start - start_idle_time);
    }
    void run_task_after(TaskBase<Options> *task) {
		Time::TimeUnit stop = Time::getTime();
        Log<Options>::log(detail::GetName::get_name(task).c_str(), start, stop);
		start_idle_time = Time::getTime();
	}
	void after_barrier() {
        Time::TimeUnit stop = Time::getTime();
        Log<Options>::add_barrier_time(stop - start_idle_time);
		start_idle_time = Time::getTime();
	}
    static void dump(const char *name) {
        Log<Options>::dump(name);
	}
};

} // namespace sg

#endif // SG_INSTR_TRACE_HPP_INCLUDED
