#include "superglue.hpp"

#include <limits>

struct Options : public DefaultOptions<Options> {
  typedef unsigned char version_t;
};

volatile size_t busy = 0;

struct MyTask : public Task<Options> {
  std::string name;
  MyTask(Handle<Options> &h) {
    registerAccess(ReadWriteAdd::write, &h);
  }
  void run() {
    if (busy != 0) {
        fprintf(stderr, "FAIL: %d %d\n", getAccess(0).getHandle()->version, getAccess(0).requiredVersion);
        assert(busy == 0);
    }
    busy = 1;
    // delay to make sure several tasks have the chance to run concurrently
    Time::TimeUnit stop = Time::getTime() + 100000;
    while (Time::getTime() < stop);
    busy = 0;
  }
};

int main() {
  const size_t num_tasks = std::numeric_limits<Options::version_t>::max()/2;

  ThreadManager<Options> tm;

  Handle<Options> h;
  for (size_t i = 0; i < num_tasks/2; ++i)
    tm.submit(new MyTask(h));
  tm.barrier();

  for (size_t i = 0; i < num_tasks; ++i)
    tm.submit(new MyTask(h));
  tm.barrier();
  return 0;
}

