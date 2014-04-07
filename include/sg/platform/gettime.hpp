#ifndef SG_GETTIME_HPP_INCLUDED
#define SG_GETTIME_HPP_INCLUDED

namespace Time {

//
// Routines for getting the current time.
//
// Defines:
//   typedef TimeUnit
//   TimeUnit getTime() // returns current time
//   TimeUnit getFreq() // returns time units per second
//

#if defined(_MSC_VER) // MICROSOFT VISUAL C++

#define NOMINMAX
#include <windows.h>
#pragma message("Using timer = Microsoft QueryPerformanceCounter()")
typedef unsigned __int64 TimeUnit;
static inline TimeUnit getTime() {
    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);
    return i.QuadPart;
}

static inline TimeUnit getFreq() {
    LARGE_INTEGER i;
    QueryPerformanceFrequency(&i);
    return i.QuadPart;
}

#elif defined(__x86_64__)

typedef unsigned long long TimeUnit;

static inline TimeUnit getTime() {
  unsigned hi, lo;

  __asm__ __volatile__ ("rdtsc\n"
         : "=a" (lo), "=d" (hi)
         :: "%rbx", "%rcx");
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static inline TimeUnit getTimeStart() {
  unsigned hi, lo;

  __asm__ __volatile__ ("cpuid\n"
                        "rdtsc\n"
         : "=a" (lo), "=d" (hi)
         :: "%rbx", "%rcx");
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static inline TimeUnit getTimeStop() {
  unsigned hi, lo;

  __asm__ __volatile__ ("rdtscp\n"
                        "mov %%edx, %0\n\t"
                        "mov %%eax, %1\n\t"
                        "cpuid\n"
          : "=r" (hi), "=r" (lo)
          :: "%rax", "%rbx", "%rcx", "%rdx");
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#else

#include <sys/time.h>
typedef unsigned long long TimeUnit;
static inline TimeUnit getTime() {
    timeval tv;
    gettimeofday(&tv, 0);
    return (tv.tv_sec*(unsigned long long )1000000+tv.tv_usec);
}

static inline TimeUnit getTimeStart() { return getTime(); }
static inline TimeUnit getTimeStop() { return getTime(); }

#endif

} // namespace Time

#endif // SG_GETTIME_HPP_INCLUDED
