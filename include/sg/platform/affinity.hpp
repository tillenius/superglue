#ifndef SG_AFFINITY_HPP_INCLUDED
#define SG_AFFINITY_HPP_INCLUDED

#include <cassert>

#ifdef __linux__
#include <unistd.h>
#include <sched.h>
#endif
#ifdef __sun
#include <unistd.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/types.h>
#include <sys/cpuvar.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace sg {

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

#else
#error Not implemented
#endif

// ===========================================================================
// ThreadAffinity
// ===========================================================================

struct ThreadAffinity {
#ifdef __sun
    static void set_affinity(affinity_cpu_set &cpu_set) {
        assert(processor_bind(P_LWPID, P_MYID, cpu_set.cpu_id, NULL) == 0);
    }
#elif __linux__
    static void set_affinity(affinity_cpu_set &cpu_set) {
        assert(sched_setaffinity(0, sizeof(cpu_set.cpu_set), &cpu_set.cpu_set) == 0);
    }
#elif __APPLE__
    static void set_affinity(affinity_cpu_set &) {
        // setting cpu affinity not supported on mac
    }
#elif _WIN32
    static void set_affinity(affinity_cpu_set &cpu_set) {
        SetThreadAffinityMask(GetCurrentThread(), cpu_set.cpu_set);
    }
#else
#error Not implemented
#endif
};

} // namespace sg

#endif // SG_AFFINITY_HPP_INCLUDED
