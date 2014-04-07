#include "sg/superglue.hpp"
#include <iostream>

const size_t numSlices = 5;
const size_t sliceSize = 100;

// Shared array divided into slices.
double data[numSlices][sliceSize];

// Define own handle type to store user-specific data in all handles.
template<typename Options>
struct MyHandle : public HandleBase<Options> {
    size_t index;
    void setIndex(size_t i) { index = i; }
};

// Specify that our handle type should replace the default handle type.
struct Options : public DefaultOptions<Options> {
    typedef MyHandle<Options> HandleType;
};

struct ScaleTask : public Task<Options> {
    double s;
    size_t a, b;
    ScaleTask(double s_, Handle<Options> &hA, Handle<Options> &hB)
    : s(s_), a(hA.index), b(hB.index)
    {
        register_access(ReadWriteAdd::read, hA);
        register_access(ReadWriteAdd::write, hB);
    }
    void run() {
        for (size_t i = 0; i < sliceSize; ++i)
            data[b][i] = s*data[a][i];
    }
};

struct SumTask : public Task<Options> {
    size_t a, b, c;
    SumTask(Handle<Options> &hA,
            Handle<Options> &hB,
            Handle<Options> &hC)
    : a(hA.index), b(hB.index), c(hC.index)
    {
        register_access(ReadWriteAdd::read, hA);
        register_access(ReadWriteAdd::read, hB);
        register_access(ReadWriteAdd::write, hC);
    }
    void run() {
        for (size_t i = 0; i < sliceSize; ++i)
            data[c][i] = data[a][i] + data[b][i];
    }
};

int main() {
    for (size_t i = 0; i < sliceSize; ++i)
        data[0][i] = 1.0;

    // Define handles for the slices
    Handle<Options> h[numSlices];

    // Set the user-defined index
    for (size_t i = 0; i < numSlices; ++i)
        h[i].setIndex(i);

    SuperGlue<Options> sg;
    sg.submit(new ScaleTask(2.0, h[0], h[1])); // h_1 = 2*h_0
    sg.submit(new ScaleTask(3.0, h[0], h[2])); // h_2 = 3*h_0
    sg.submit(new SumTask(h[0], h[1], h[3]));  // h_3 = h_0+h_1
    sg.submit(new SumTask(h[1], h[2], h[4]));  // h_4 = h_1+h_2

    // Wait for all tasks to finish
    sg.barrier();

    // The data may be accessed here, after the barrier
    std::cout << "result=[" << data[0][0] << " "  << data[1][0] << " "
              << data[2][0] << " " << data[3][0] << " " << data[4][0]
              << "]" << std::endl;
    return 0;
}
