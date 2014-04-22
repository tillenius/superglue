#include "sg/superglue.hpp"
#include "sg/core/defaults.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <cstddef>

using namespace std;

//
// This example uses tasks with variable number of dependencies, and mixes
// these together with tasks with a fixed number of dependencies.
//
// What the application is doing is first initializing all elements of a
// matrix with ones (one task per element), and then replace the diagonal
// element with the sum of all elements to the left of the diagonal.
//
// This is just an example of how to create tasks with variable number of
// dependencies. The tasks are too small to run efficiently.
//

const size_t N = 6;

//===========================================================================
// Task Library Options
//===========================================================================
struct Options : public DefaultOptions<Options> {};

//===========================================================================
// Tasks
//===========================================================================

class InitBlock : public Task<Options, 1> {
    double *data;
public:
    InitBlock(double *data_, Handle<Options> *h, size_t i, size_t j)
      : data(&data_[j*N+i]) {
        register_access(ReadWriteAdd::write, h[j*N+i]);
    }

    void run() {
        *data = 1.0;
    }
};

class DiagTask : public Task<Options> {
private:
    double *data;
    size_t j;

public:
    DiagTask(double *data_, Handle<Options> *h, size_t j_)
      : data(data_), j(j_) {
        for (size_t i = 0; i < j; ++i)
            register_access(ReadWriteAdd::read, h[j*N+i]);
        register_access(ReadWriteAdd::write, h[j*N+j]);
    }

    void run() {
        double tmp = 0.0;
        for (size_t i = 0; i < j; ++i)
            tmp += data[j*N+i];
        data[j*N+j] = tmp;
    }
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

    Handle<Options> *h = new Handle<Options>[N*N];
    double *data = new double[N*N];

    SuperGlue<Options> sg(num_threads);

    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < N; ++j)
            sg.submit(new InitBlock(data, h, i, j));

    for (size_t i = 0; i < N; ++i)
        sg.submit(new DiagTask(data, h, i));

    sg.barrier();

    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j)
            std::cout << data[j*N+i] << " ";
        std::cout << std::endl;
    }

    delete [] data;
    delete [] h;

    return 0;
}
