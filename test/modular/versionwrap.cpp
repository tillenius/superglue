#include "sg/superglue.hpp"

#include "sg/platform/gettime.hpp"
#include <limits>
#include <cstdio>

struct Options : public DefaultOptions<Options> {
  typedef unsigned char version_t;
};

volatile size_t busy = 0;

struct MyTask : public Task<Options> {
  std::string name;
  MyTask(Handle<Options> &h) {
    register_access(ReadWriteAdd::write, h);
  }
  void run() {
    if (busy != 0) {
        fprintf(stderr, "FAIL: %d %d\n", get_access(0).get_handle()->version, get_access(0).required_version);
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

  SuperGlue<Options> sg;

  Handle<Options> h;
  for (size_t i = 0; i < num_tasks/2; ++i)
    sg.submit(new MyTask(h));
  sg.barrier();

  for (size_t i = 0; i < num_tasks; ++i)
    sg.submit(new MyTask(h));
  sg.barrier();
  return 0;
}

