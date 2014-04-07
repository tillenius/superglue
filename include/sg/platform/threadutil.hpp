#ifndef SG_THREADUTIL_HPP_INCLUDED
#define SG_THREADUTIL_HPP_INCLUDED

#ifdef __linux__
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

namespace sg {

#ifndef _WIN32
#define PTHREADS
typedef pthread_t ThreadIDType;
#else
typedef DWORD ThreadIDType;
#endif

// ===========================================================================
// ThreadUtil: Utility functions
// ===========================================================================
class ThreadUtil {
public:

    // static int get_num_cpus()

    #if defined(__sun)
    static int get_num_cpus() {
        int numCPUs = (processorid_t) sysconf(_SC_NPROCESSORS_ONLN);
        int online = 0;
        for (int i = 0; i < numCPUs; ++i)
            if (p_online(i, P_STATUS) == P_ONLINE)
                ++online;
        return online;
    }
    #elif defined(__linux__) || defined(__APPLE__)
    static int get_num_cpus() { return (int) sysconf(_SC_NPROCESSORS_ONLN); }
    #elif defined(_WIN32)
    static int get_num_cpus() {
        SYSTEM_INFO m_si = {0};
        GetSystemInfo(&m_si);
        return (int) m_si.dwNumberOfProcessors;
    }
    #else
    #error Not implemented for this platform
    #endif

    // static ThreadIDType get_current_thread_id()

    #ifdef PTHREADS
    static ThreadIDType get_current_thread_id() { return pthread_self(); }
    #elif _WIN32
    static ThreadIDType get_current_thread_id() { return GetCurrentThreadId(); }
    #else
    #error Not implemented for this platform
    #endif
};

} // namespace sg

#endif // SG_THREADUTIL_HPP_INCLUDED
