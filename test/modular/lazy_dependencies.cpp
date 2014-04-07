#include "sg/superglue.hpp"

#include "sg/option/depcheck_lazy.hpp"

struct Options : public DefaultOptions<Options> {
    typedef LazyDependencyChecking<Options> DependencyChecking;
};

struct RunOrder {
    SpinLock lock;
    std::vector<size_t> order;
    void push_back(size_t i) {
        SpinLockScoped hold(lock);
        order.push_back(i);
    }
    size_t operator[](size_t i) { return order[i]; }
    size_t size() { return order.size(); }
};

RunOrder r;

struct Read : public Task<Options, 1> {
    size_t i;
    Read(Handle<Options> &h, size_t i_) : i(i_) { register_access(ReadWriteAdd::read, h); }
    void run() { r.push_back(i); }
};

struct Write : public Task<Options, 1> {
    size_t i;
    Write(Handle<Options> &h, size_t i_) : i(i_) { register_access(ReadWriteAdd::write, h); }
    void run() { r.push_back(i); }
};

struct Add : public Task<Options, 1> {
    size_t i;
    Add(Handle<Options> &h, size_t i_) : i(i_) { register_access(ReadWriteAdd::add, h); }
    void run() { r.push_back(i); }
};

int main() {
  SuperGlue<Options> sg;
  Handle<Options> h;
  sg.submit(new Read(h, 0));
  sg.submit(new Write(h, 1));
  sg.submit(new Add(h, 2));
  sg.submit(new Read(h, 3));
  sg.submit(new Add(h, 4));
  sg.submit(new Write(h, 5));
  sg.submit(new Write(h, 6));
  sg.submit(new Read(h, 7));
  sg.barrier();
  assert(r.size() == 8);
  for (size_t i = 0; i < r.size(); ++i)
    assert(r[i] == i);
  return 0;
}
