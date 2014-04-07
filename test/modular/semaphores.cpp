#include "sg/superglue.hpp"

//#include "sg/option/depcheck_lazy.hpp"
#include "sg/option/instr_trace.hpp"

struct Options : public DefaultOptions<Options> {
  typedef Trace<Options> Instrumentation;
  typedef Enable TaskName;
};

void wait() {
  Time::TimeUnit stop = Time::getTime() + 100000;
  while (Time::getTime() < stop)
    Atomic::rep_nop();
}

struct Req1 : public Task<Options, 1> {
  Req1(Resource<Options> &r) { require(r); }
  void run() { wait(); }
  std::string get_name() { return "req1"; }
};

struct Req3 : public Task<Options, 1> {
  Req3(Resource<Options> &r) { require(r, 3); }
  void run() { wait(); }
  std::string get_name() { return "req3"; }
};

int main() {
  SuperGlue<Options> sg;
  Handle<Options> h;
  Resource<Options> res(7);

  for (int i = 0; i < 1000; ++i)
    sg.submit(new Req3(res));

  for (int i = 0; i < 1000; ++i)
    sg.submit(new Req1(res));

  sg.barrier();
  // assert(r.size() == 8);
  // for (size_t i = 0; i < r.size(); ++i)
  //  assert(r[i] == i);

  Log<Options>::dump("trace.log");
  return 0;
}
