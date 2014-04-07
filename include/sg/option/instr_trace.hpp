#ifndef SG_INSTR_TRACE_HPP_INCLUDED
#define SG_INSTR_TRACE_HPP_INCLUDED

#include "sg/option/log.hpp"

namespace sg {

template<typename Options>
class TaskExecutor;

template<typename Options>
struct Trace {
    Time::TimeUnit start, stop;
    Trace(int threadid) {
        Log<Options>::register_thread(threadid);
    }
    void run_task_before(TaskBase<Options> *) {
        start = Time::getTime();
    }
    void run_task_after(TaskBase<Options> *task) {
        stop = Time::getTime();
        Log<Options>::log(detail::GetName::get_name(task).c_str(), start, stop);
    }
    static void dump(const char *name) {
        Log<Options>::dump(name);
    }
};

} // namespace sg

#endif // SG_INSTR_TRACE_HPP_INCLUDED
