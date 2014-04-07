#include "sg/superglue.hpp"
#include <iostream>
#include <cstdio>

struct Options : public DefaultOptions<Options> {};

// User-defined datatype that includes a handle for dependency management
struct MyFloatData {
    Handle<Options> handle;
    double value;
};

// Another user-defined datatype that also includes a handle
struct MyTextData {
    Handle<Options> handle;
    char text[10];
};

struct ScaleTask : public Task<Options> {
    double s, &a, &b;
    ScaleTask(double s_, MyFloatData &a_, MyFloatData &b_)
    : s(s_), a(a_.value), b(b_.value)
    {
        register_access(ReadWriteAdd::read, a_.handle);
        register_access(ReadWriteAdd::write, b_.handle);
    }
    void run() {
        b = s*a;
    }
};

struct ToStrTask : public Task<Options> {
    double &a;
    char *b;
    ToStrTask(MyFloatData &a_, MyTextData &b_)
    : a(a_.value), b(b_.text)
    {
        register_access(ReadWriteAdd::read, a_.handle);
        register_access(ReadWriteAdd::write, b_.handle);
    }
    void run() {
        sprintf(b, "%f", a);
    }
};

int main() {
    // User-defined datatypes including handles
    MyFloatData a, b, c;
    MyTextData bstr, cstr;
    a.value = 1.0;

    SuperGlue<Options> sg;
    sg.submit(new ScaleTask(2.0, a, b)); // b = 2*a
    sg.submit(new ScaleTask(3.0, a, c)); // c = 3*a
    sg.submit(new ToStrTask(b, bstr));
    sg.submit(new ToStrTask(c, cstr));
    sg.barrier();

    std::cout << "b=" << bstr.text << " c=" << cstr.text << std::endl;
    return 0;
}

