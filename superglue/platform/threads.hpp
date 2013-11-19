#ifndef __THREADS_HPP__
#define __THREADS_HPP__

// ===========================================================================
// Defines Threads interface, and contains platform specific threading code
// ===========================================================================

#include <cassert>

#ifdef __linux__
#include <stdlib.h>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#endif
#ifdef __sun
#include <unistd.h>
#include <cstdlib>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/types.h>
#include <sys/cpuvar.h>
#include <pthread.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#ifndef _WIN32
#define PTHREADS
typedef pthread_t ThreadType;
typedef pthread_t ThreadIDType;
#else
typedef HANDLE ThreadType;
typedef DWORD ThreadIDType;
#endif


// ===========================================================================
// affinity_cpu_set: basically a wrapper around cpu_set_t.
// ===========================================================================

#ifdef __linux__

struct affinity_cpu_set {
    cpu_set_t cpu_set;
    affinity_cpu_set() {
        CPU_ZERO(&cpu_set);
    }
    void set(int cpu) {
        CPU_SET(cpu, &cpu_set);
    }
};

#elif __sun

// On Solaris, threads are only pinned to a single thread,
// the last one set in cpu_set.
struct affinity_cpu_set {
    int cpu_id;
    void set(int cpu) {
        cpu_id = cpu;
    }
};
  
#elif __APPLE__

struct affinity_cpu_set {
    void set(int) {}
};

#elif _WIN32

struct affinity_cpu_set {
    DWORD_PTR cpu_set;
    affinity_cpu_set() : cpu_set(0) {}
    void set(int cpu) {
        cpu_set |= 1<<cpu;
    }
};

#endif

namespace detail {

// ===========================================================================
// ThreadImplAux: pthread implementations depending on platform
// ===========================================================================
#ifdef __sun
struct ThreadImplAux {
    static void setAffinity(affinity_cpu_set &cpu_set) {
        assert(processor_bind(P_LWPID, P_MYID, cpu_set.cpu_id, NULL) == 0);
    }

    static int getNumCPUs() {
        int numCPUs = (processorid_t) sysconf(_SC_NPROCESSORS_ONLN);
        int online = 0;
        for (int i = 0; i < numCPUs; ++i)
            if (p_online(i, P_STATUS) == P_ONLINE)
                ++online;
        return online;
    }
};
#elif __linux__
struct ThreadImplAux {
    static void setAffinity(affinity_cpu_set &cpu_set) {
        assert(sched_setaffinity(0, sizeof(cpu_set.cpu_set), &cpu_set.cpu_set) == 0);
    }
    static int getNumCPUs() { return (int) sysconf(_SC_NPROCESSORS_ONLN); }
};
#elif __APPLE__
struct ThreadImplAux {
    static void setAffinity(affinity_cpu_set &) {
        // setting cpu affinity not supported on mac
    }
    static int getNumCPUs() { return (int) sysconf(_SC_NPROCESSORS_ONLN); }
};
#elif _WIN32
struct ThreadImplAux {
    static void setAffinity(affinity_cpu_set &cpu_set) {
        SetThreadAffinityMask(GetCurrentThread(), cpu_set.cpu_set);
        // The thread should be rescheduled immediately:
        // "If the new thread affinity mask does not specify the processor that is
        //  currently running the thread, the thread is rescheduled on one of the
        //  allowable processors."
    }
    static int getNumCPUs() {
        SYSTEM_INFO m_si = {0};
        GetSystemInfo(&m_si);
        return (int) m_si.dwNumberOfProcessors;
    }
};
#endif

// ===========================================================================
// ThreadImpl: Implementations depending on platform
// ===========================================================================
#ifdef PTHREADS
  struct ThreadImpl {
      static void exit() { pthread_exit(0); }
      static void sleep(unsigned int millisec) { usleep(millisec * 1000); }
      static ThreadIDType getCurrentThreadId() { return pthread_self(); }
      static void join(ThreadType thread) {
          void *status;
          pthread_join(thread, &status);
      }
  };

#elif _WIN32
  struct ThreadImpl {
      static void exit() { ExitThread(0); }
      static void sleep(unsigned int millisec) { Sleep(millisec); }
      static ThreadIDType getCurrentThreadId() { return GetCurrentThreadId(); }
      static void join(ThreadType thread) { WaitForSingleObject(thread, INFINITE); }

  };
#else
#error Not implemented for this platform
#endif

} // namespace detail

// ===========================================================================
// ThreadUtil: Utility functions
// ===========================================================================
class ThreadUtil {
public:
    static void exit() { detail::ThreadImpl::exit(); }
    static void sleep(unsigned int millisec) { detail::ThreadImpl::sleep(millisec); }
    static int getNumCPUs() { return detail::ThreadImplAux::getNumCPUs(); }
    static ThreadIDType getCurrentThreadId() { return detail::ThreadImpl::getCurrentThreadId(); }
    static void setAffinity(affinity_cpu_set &cpu_set) { detail::ThreadImplAux::setAffinity(cpu_set); }
};

namespace detail {
#ifdef _WIN32
DWORD WINAPI spawnThread(LPVOID arg);
#else
extern "C" void *spawnThread(void *arg);
#endif
}

namespace detail {

// ===========================================================================
// Native part of the Thread class
// ===========================================================================
#ifdef _WIN32
template<typename Thread>
class ThreadNative {
public:
    void start() {
        DWORD threadID;
        Thread *this_(static_cast<Thread *>(this));
        this_->thread = CreateThread(NULL, 0, &detail::spawnThread, this_, 0, &threadID);
    }
};
#else

template<typename Thread>
class ThreadNative {
public:
    void start() {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        Thread *this_(static_cast<Thread *>(this));
        assert(pthread_create(&this_->thread, &attr, &detail::spawnThread, this_) == 0);
        pthread_attr_destroy(&attr);
    }
};
#endif

} // namespace detail

// ===========================================================================
// Thread
// ===========================================================================
class Thread : public detail::ThreadNative<Thread> {
private:
    template<typename> friend class detail::ThreadNative;

    Thread(const Thread &);
    const Thread &operator=(const Thread &);

protected:
    ThreadType thread;

public:
    Thread() {}

    virtual ~Thread() {};
    virtual void run() = 0;

    void join() { detail::ThreadImpl::join(thread); }
};

namespace detail {
#ifdef _WIN32
DWORD WINAPI spawnThread(LPVOID arg) {
    Thread *t = static_cast<Thread *>(arg);
    t->run();
    ThreadUtil::exit();
    return 0;
}
#else
extern "C" void *spawnThread(void *arg) {
    Thread *t = static_cast<Thread *>(arg);
    t->run();
    ThreadUtil::exit();
    return 0;
}
#endif
} // namespace detail


#endif // __THREADS_HPP__


