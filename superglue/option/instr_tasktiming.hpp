#ifndef __INSTR_TASKTIMING_HPP__
#define __INSTR_TASKTIMING_HPP__

template<typename Options>
class TaskExecutor;

template<typename Options>
struct TaskExecutorTiming {
    Time::TimeUnit start, stop;
    static void init(TaskExecutor<Options> &te) {}
    static void destroy(TaskExecutor<Options> &te) {}
    static void finalize() {}
    void runTaskBefore(TaskBase<Options> *) {
        start = Time::getTime();
    }
    void runTaskAfter(TaskBase<Options> *task) {
        stop = Time::getTime();
        Log<Options>::log(detail::GetName::getName(task).c_str(), start, stop);
    }
    static void taskNotRunDeps() {}
    static void taskNotRunLock() {}
};

#endif // __INSTR_TASKTIMING_HPP__
