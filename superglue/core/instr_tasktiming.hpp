#ifndef __INSTR_TASKTIMING_HPP__
#define __INSTR_TASKTIMING_HPP__

template<typename Options>
struct TaskExecutorTiming {
    Time::TimeUnit start, stop;
    static void init() {}
    static void destroy() {}
    void runTaskBefore(TaskBase<Options> *) {
        start = Time::getTime();
    }
    void runTaskAfter(TaskBase<Options> *task) {
        stop = Time::getTime();
        Log<Options>::addEvent(detail::GetName::getName(task).c_str(), start, stop);
    }
    static void taskNotRunDeps() {}
    static void taskNotRunLock() {}
    static void setMainThreadFlag() {}
};

#endif // __INSTR_TASKTIMING_HPP__
