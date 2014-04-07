#include "sg/superglue.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <cmath>
#include <cstddef>

using namespace std;

// This example defines a new access type: Mul, to define a
// commutative multiplicative access that can be reordered
// among other accesses of the same type, as with the Add
// access type.

class ReadWriteAddMul {
public:
    enum Type { read = 0, write, add, mul, num_accesses };
    template<int n> struct AccessType {};
};

template<> struct ReadWriteAddMul::AccessType<ReadWriteAddMul::read> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 1 };
};

template<> struct ReadWriteAddMul::AccessType<ReadWriteAddMul::write> {
    enum { commutative = 0 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

template<> struct ReadWriteAddMul::AccessType<ReadWriteAddMul::add> {
    enum { commutative = 1 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

template<> struct ReadWriteAddMul::AccessType<ReadWriteAddMul::mul> {
    enum { commutative = 1 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

//===========================================================================
// Task Library Options
//===========================================================================
struct Options : public DefaultOptions<Options> {
    typedef ReadWriteAddMul AccessInfoType;
    typedef Enable Lockable;
    typedef Enable TaskName;
};

Handle<Options> handle;
volatile double data = 0.0;

//===========================================================================
// Tasks
//===========================================================================
class TaskSet : public Task<Options, 1> {
private:
    double value;
public:
    TaskSet(double value_) : value(value_) {
        register_access(ReadWriteAddMul::write, handle);
    }

    void run() {
        data = value;
        std::stringstream ss;
        ss << "=" << value << "="<<data<<" "<<std::endl;
        std::cerr << ss.str();
    }
    std::string get_name() { return "Set"; }
};

class TaskAdd : public Task<Options, 1> {
private:
    double value;
public:
    TaskAdd(double value_) : value(value_) {
        register_access(ReadWriteAddMul::add, handle);
    }

    void run() {
        data += value;
        std::stringstream ss;
        ss << "+" << value << "="<<data<<" "<<std::endl;
        std::cerr << ss.str();
    }
    std::string get_name() { return "Add"; }
};

class TaskMul : public Task<Options, 1> {
private:
    double value;
public:
    TaskMul(double value_) : value(value_) {
        register_access(ReadWriteAddMul::mul, handle);
    }

    void run() {
        data *= value;
        std::stringstream ss;
        ss << "*" << value << "="<<data<<" "<<std::endl;
        std::cerr << ss.str();
    }
    std::string get_name() { return "Mul"; }
};

class TaskPrint : public Task<Options, 1> {
private:
public:
    TaskPrint() {
        register_access(ReadWriteAddMul::read, handle);
    }
    void run() {
        std::stringstream ss;
        ss << "Result=" << data << std::endl;
        std::cerr << ss.str();
    }
    std::string get_name() { return "Print"; }
};

//===========================================================================
// main
//===========================================================================
int main(int argc, char *argv[]) {

    int num_threads = -1;
    if (argc == 2) {
        num_threads = (size_t) atoi(argv[1]);
    }
    else if (argc != 1) {
        printf("usage: %s [num_cores]\n", argv[0]);
        exit(0);
    }

    SuperGlue<Options> sg(num_threads);
    // 1 * 2 * 3 * 4 + 5 + 6 + 7 = 42
    sg.submit(new TaskSet(1.0));
    sg.submit(new TaskMul(2.0));
    sg.submit(new TaskMul(3.0));
    sg.submit(new TaskMul(4.0));
    sg.submit(new TaskAdd(5.0));
    sg.submit(new TaskAdd(6.0));
    sg.submit(new TaskAdd(7.0));
    sg.submit(new TaskPrint());
    sg.barrier();

    return 0;
}
