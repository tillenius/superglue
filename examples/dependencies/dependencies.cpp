#include "superglue.hpp"
#include <iostream>

const size_t numSlices = 5;
const size_t sliceSize = 100;

struct Options : public DefaultOptions<Options> {};

struct ScaleTask : public Task<Options> {
    double s, *a, *b;
    ScaleTask(double s_,
              double *a_, Handle<Options> &hA,
              double *b_, Handle<Options> &hB)
    : s(s_), a(a_), b(b_)
    {
        registerAccess(ReadWriteAdd::read, &hA);
        registerAccess(ReadWriteAdd::write, &hB);
    }
    void run() {
        for (size_t i = 0; i < sliceSize; ++i)
            b[i] = s*a[i];
    }
};

struct SumTask : public Task<Options> {
    double *a, *b, *c;
    SumTask(double *a_, Handle<Options> &hA,
            double *b_, Handle<Options> &hB,
            double *c_, Handle<Options> &hC)
    : a(a_), b(b_), c(c_)
    {
        registerAccess(ReadWriteAdd::read, &hA);
        registerAccess(ReadWriteAdd::read, &hB);
        registerAccess(ReadWriteAdd::write, &hC);
    }
    void run() {
        for (size_t i = 0; i < sliceSize; ++i)
            c[i] = a[i]+b[i];
    }
};

int main() {
    // Shared array divided into slices
    double data[numSlices][sliceSize];

    for (size_t i = 0; i < sliceSize; ++i)
        data[0][i] = 1.0;

    // Define handles for the slices
    Handle<Options> h[numSlices];

    ThreadManager<Options> tm;
    tm.submit(new ScaleTask(2.0, data[0], h[0], data[1], h[1])); // h_1 = 2*h_0
    tm.submit(new ScaleTask(3.0, data[0], h[0], data[2], h[2])); // h_2 = 3*h_0
    tm.submit(new SumTask(data[0], h[0], data[1], h[1], data[3], h[3])); // h_3 = h_0+h_1
    tm.submit(new SumTask(data[1], h[1], data[2], h[2], data[4], h[4])); // h_4 = h_1+h_2

    // Wait for all tasks to finish
    tm.barrier();

    // The data may be accessed here, after the barrier
    std::cout << "result=[" << data[0][0] << " "  << data[1][0] << " "
              << data[2][0] << " " << data[3][0] << " " << data[4][0]
              << "]" << std::endl;
    return 0;
}
