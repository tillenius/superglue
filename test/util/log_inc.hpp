#include <pthread.h>
#include <string.h>

///////////////////// TIMING

typedef unsigned long long LOG_TimeUnit;

static inline LOG_TimeUnit LOG_getTimeStart() {
  unsigned hi, lo;

  __asm__ __volatile__ ("cpuid\n"
                        "rdtsc\n"
         : "=a" (lo), "=d" (hi)
         :: "%rbx", "%rcx");
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static inline LOG_TimeUnit LOG_getTimeStop() {
  unsigned hi, lo;

  __asm__ __volatile__ ("rdtscp\n"
                        "mov %%edx, %0\n\t"
                        "mov %%eax, %1\n\t"
                        "cpuid\n"
          : "=r" (hi), "=r" (lo)
          :: "%rax", "%rbx", "%rcx", "%rdx");
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#ifdef LOGGING
///////////////////// LOGGING

#define LOG_MAX_ENTRIES 1000000

static inline unsigned long LOG_getThread() {
  return pthread_self();
}

typedef struct {
  LOG_TimeUnit start, length;
  char text[64 - 2*sizeof(LOG_TimeUnit)- sizeof(unsigned long)];
  unsigned long thread;
} LOG_entry;

LOG_entry LOG_data[LOG_MAX_ENTRIES];
size_t LOG_ptr;

void LOG_init() {
}

static inline void LOG_add(const char *text, LOG_TimeUnit start, LOG_TimeUnit stop) {
  size_t i = __sync_fetch_and_add(&LOG_ptr, 1);
  strcpy(LOG_data[i].text, text);
  LOG_data[i].start = start;
  LOG_data[i].length = stop-start;
  LOG_data[i].thread = LOG_getThread();
}

static void LOG_dump(const char *filename) {
  FILE *out = fopen(filename, "w");
  fprintf(out, "LOG 2\n");
  LOG_TimeUnit minimum = LOG_data[0].start;
  for (size_t i = 0; i < LOG_ptr; ++i) {
      if (LOG_data[i].start < minimum)
          minimum = LOG_data[i].start;
  }
  for (size_t i = 0; i < LOG_ptr; ++i) {
      fprintf(out, "%lu: %llu %llu %s\n",
              LOG_data[i].thread,
              LOG_data[i].start-minimum,
              LOG_data[i].length,
              LOG_data[i].text);
  }
  fclose(out);
}

#ifdef LOGGING_PERF
///////////////////// PERFORMANCE_COUNTERS

int LOG_fd;

unsigned long long LOG_readCounter() {
    unsigned long long count;
    size_t res = read(LOG_fd, &count, sizeof(unsigned long long));
    if (res != sizeof(unsigned long long)) {
        fprintf(stderr, "read() failed %d", res);
        exit(1);
    }
    return count;
}

void LOG_init_perf(unsigned int type, unsigned long long config) {
    struct perf_event_attr attr = {0};
    attr.type = type;
    attr.config = config;
    attr.inherit = 1;
    attr.disabled = 1;
    attr.size = sizeof(struct perf_event_attr);
    LOG_fd = syscall(__NR_perf_event_open, attr, 0, -1, -1, 0);

    if (LOG_fd < 0) {
        fprintf(stderr, "sys_perf_event_open failed %d", fd);
        exit(1);
    }
}

unsigned long long LOG_getCacheStart() {
    unsigned long long count;
    count = LOG_readCounter();
    ioctl(LOG_fd, PERF_EVENT_IOC_ENABLE);
    return count;
}
unsigned long long LOG_getCacheStop() {
    unsigned long long count;
    ioctl(LOG_fd, PERF_EVENT_IOC_DISABLE);
    count = LOG_readCounter();
    return count;
}

#else // LOGGING_PERF
#define LOG_init_perf(a,b)
#define LOG_getCacheStart() 0
#define LOG_getCacheStop() 0
#endif

#else // LOGGING
#define LOG_add(a,b,c)
#define LOG_dump(a)
#define LOG_init_perf(a,b)
#define LOG_getCacheStart() 0
#define LOG_getCacheStop() 0
#endif
