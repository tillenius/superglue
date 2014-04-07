#ifndef SG_INSTR_PROCSTAT_HPP_INCLUDED
#define SG_INSTR_PROCSTAT_HPP_INCLUDED

#include "sg/option/log.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace sg {

template<typename Options>
struct PerfProcStat {

    Time::TimeUnit start, stop;

    static int gettid() {
        return syscall(SYS_gettid);
    }

    void run_task_before(TaskBase<Options> *) {
        start = Time::getTime();
    }
    void run_task_after(TaskBase<Options> *task) {
        stop = Time::getTime();

        pid_t pid = ::getpid();
        pid_t tid = gettid();

        char procFilename[256];
        sprintf(procFilename, "/proc/%d/task/%d/stat",pid,tid) ;

        char buffer[1024];

        int fd = open(procFilename, O_RDONLY, 0);
        int num_read = read(fd, buffer, 1023);
        close(fd);
        buffer[num_read-1] = '\0';

        Log<Options>::log((detail::GetName::get_name(task) + buffer).c_str(), start, stop);
    }
};

} // namespace sg

#endif // SG_INSTR_PROCSTAT_HPP_INCLUDED
