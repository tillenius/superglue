#include "superglue.hpp"
#include <iostream>

template<typename Options>
struct MyHandle : public HandleDefault<Options> {
    int data;
};

struct Options : public DefaultOptions<Options> {
    // This option must be enabled in order to make task submission thread safe
    typedef Enable ThreadSafeSubmit;

    // This changes the handle to contain data
    typedef MyHandle<Options> HandleType;
};

ThreadManager<Options> tm;

struct Producer : public Task<Options> {
    Handle<Options> *out;

    Producer(Handle<Options> *out_) : out(out_) {
        registerAccess(ReadWriteAdd::write, out);
    }

    void run() {
        out->data = 10;
    }
};

// task that uses temporary variable and deletes it when finished
struct ConsumeAndDelete : public Task<Options> {
    Handle<Options> *in;
    ConsumeAndDelete(Handle<Options> *in_) : in(in_) {
        registerAccess(ReadWriteAdd::write, in); // register as write
    }
    void run() {
        std::stringstream ss;
        ss << "consume&delete " << in->data << " " << in << std::endl;
        std::cerr << ss.str();
        // must not delete handle here.
        // registered handles may only be deleted in the destructor
    }
    ~ConsumeAndDelete() {
        delete in;
    }
};

// task that uses temporary variable
struct Consumer : public Task<Options> {
    Handle<Options> *in;

    Consumer(Handle<Options> *in_) : in(in_) {
        registerAccess(ReadWriteAdd::read, in);
    }
    void run() {
        std::stringstream ss;
        ss << "consume " << in->data << " " << in << std::endl;
        std::cerr << ss.str();
    }
};

// task to delete temporary variable when all tasks are finished with it
struct Deleter : public Task<Options> {
    Handle<Options> *in;
    Deleter(Handle<Options> *in_) : in(in_) {
        registerAccess(ReadWriteAdd::write, in);
    }
    void run() {
        std::stringstream ss;
        ss << "delete " << in->data << " " << in << std::endl;
        std::cerr << ss.str();
    }
    ~Deleter() {
        delete in;
    }
};

struct OuterTask : public Task<Options> {

    void run() {
        {
            Handle<Options> *temp(new Handle<Options>());
            tm.submit(new Producer(temp));
            tm.submit(new ConsumeAndDelete(temp));
        }

        {
            Handle<Options> *temp(new Handle<Options>());
            tm.submit(new Producer(temp));
            tm.submit(new Consumer(temp));
            tm.submit(new Consumer(temp));
            tm.submit(new Consumer(temp));
            tm.submit(new Deleter(temp));
        }
    }
};

int main() {
    for (size_t i(0); i != 10; ++i)
        tm.submit(new OuterTask());
    tm.barrier();
    return 0;
}
