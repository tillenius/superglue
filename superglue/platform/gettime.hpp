#ifndef __GETTIME_HPP__
#define __GETTIME_HPP__

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

#else
#include <sys/time.h>
//typedef long int TimeUnit;
//static inline TimeUnit getTime() {
//    timeval tv;
//    gettimeofday(&tv, 0);
//    return (tv.tv_sec*1000000+tv.tv_usec);
//}
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


#endif

} // namespace Time

#endif // __GETTIME_HPP__
