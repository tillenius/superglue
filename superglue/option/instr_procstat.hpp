
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

template<typename Options>
struct PerfProcStat {

    Time::TimeUnit start, stop;

    int gettid() {
        return syscall(SYS_gettid);
    }

    static void init(TaskExecutor<Options> &te) {}
    static void destroy(TaskExecutor<Options> &te) {}
    static void finalize() {}
    void runTaskBefore(TaskBase<Options> *) {
        start = Time::getTime();
    }
    void runTaskAfter(TaskBase<Options> *task) {
        stop = Time::getTime();

        pid_t pid = ::getpid();
        pid_t tid = ::gettid();

        char procFilename[256];
        sprintf(procFilename, "/proc/%d/task/%d/stat",pid,tid) ;

        char buffer[1024];

        int fd = open(procFilename, O_RDONLY, 0);
        int num_read = read(fd, buffer, 1023);
        close(fd);
        buffer[num_read-1] = '\0';

        Log<Options>::addEvent((detail::GetName::getName(task) + buffer).c_str(), start, stop);
    }
    static void taskNotRunDeps() {}
    static void taskNotRunLock() {}
};
