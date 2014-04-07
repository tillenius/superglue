//
// Cholesky example using MKL.
//
// In this example, the Handle class is expanded to contain two indicies.
//
// Based on a Cholesky example from SMPSs, with the following copyright notice:
//
// Copyright (c) 2008, BSC (Barcelon Supercomputing Center)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the <organization> nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY BSC ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "sg/superglue.hpp"
#include "sg/option/instr_trace.hpp"

#ifdef USE_PRIO
#include "sg/option/taskqueue_prio.hpp"
#endif

#include <math.h>
#ifdef USE_MKL
#include <mkl.h>
#endif
#ifdef USE_ACML
#include <acml.h>
#endif
#include <iostream>

double **Adata;
size_t N, NB, DIM;

// Expand handle to include two indexes
template<typename Options>
struct MyHandle : public HandleBase<Options> {
    size_t i, j;
    void set(size_t i_, size_t j_) { i = i_; j = j_; }
    size_t geti() { return i; }
    size_t getj() { return j; }
};

// ==========================================================================
// Task Library Options
//
// Set task library options:
//   * Use user expanded handles (that includes two indexes)
//   * Enable logging (generate execution trace)
// ==========================================================================
struct Options : public DefaultOptions<Options> {
    typedef MyHandle<Options> HandleType;
    typedef Trace<Options> Instrumentation;
    typedef Enable TaskName;
#ifdef USE_PRIO
    typedef TaskQueuePrioUnsafe<Options> TaskQueueUnsafeType;
#endif
};

// ==========================================================================
// Tasks
// ==========================================================================
struct gemm : public Task<Options, 3> {
    gemm(Handle<Options> &h1, Handle<Options> &h2, Handle<Options> &h3) {
        register_access(ReadWriteAdd::read, h1);
        register_access(ReadWriteAdd::read, h2);
        register_access(ReadWriteAdd::write, h3);
    }
    void run() {

        double *a(Adata[get_access(0).get_handle()->geti()*DIM + get_access(0).get_handle()->getj()]);
        double *b(Adata[get_access(1).get_handle()->geti()*DIM + get_access(1).get_handle()->getj()]);
        double *c(Adata[get_access(2).get_handle()->geti()*DIM + get_access(2).get_handle()->getj()]);

#ifdef USE_MKL
        int nb=NB;
        double DONE=1.0, DMONE=-1.0;
        dgemm("N", "T", &nb, &nb, &nb, &DMONE, a, &nb, b, &nb, &DONE, c, &nb);
#endif
#ifdef USE_ACML
        dgemm('N', 'T', NB, NB, NB, -1.0, a, NB, b, NB, 1.0, c, NB);
#endif
    }
    std::string get_name() { return "gemm"; }
};

struct syrk : public Task<Options, 2> {
    syrk(Handle<Options> &h1, Handle<Options> &h2) {
        register_access(ReadWriteAdd::read, h1);
        register_access(ReadWriteAdd::write, h2);

#ifdef USE_PRIO
	is_prioritized = true;
#endif

    }
    void run() {

        double *a(Adata[get_access(0).get_handle()->geti()*DIM + get_access(0).get_handle()->getj()]);
        double *c(Adata[get_access(1).get_handle()->geti()*DIM + get_access(1).get_handle()->getj()]);
#ifdef USE_MKL
        int nb = NB;
        double DONE=1.0, DMONE=-1.0;
        dsyrk("L", "N", &nb, &nb, &DMONE, a, &nb, &DONE, c, &nb);
#endif
#ifdef USE_ACML
        dsyrk('L', 'N', NB, NB, -1.0, a, NB, 1.0, c, NB);
#endif
    }
    std::string get_name() { return "syrk"; }
};

struct potrf : public Task<Options, 1> {
    potrf(Handle<Options> &h1) {
        register_access(ReadWriteAdd::write, h1);

#ifdef USE_PRIO
	is_prioritized = true;
#endif

    }
    void run() {
        double *a(Adata[get_access(0).get_handle()->geti()*DIM + get_access(0).get_handle()->getj()]);
        int info = 0;
#ifdef USE_MKL
        int nb = NB;
        dpotrf("L", &nb, a, &nb, &info);
#endif
#ifdef USE_ACML
        dpotrf('L', NB, a, NB, &info);
#endif
    }
    std::string get_name() { return "potrf"; }
};

struct trsm : public Task<Options, 2> {
    trsm(Handle<Options> &h1, Handle<Options> &h2) {
        register_access(ReadWriteAdd::read, h1);
        register_access(ReadWriteAdd::write, h2);

#ifdef USE_PRIO
        is_prioritized = true;
#endif

    }
    void run() {
        double *a(Adata[get_access(0).get_handle()->geti()*DIM + get_access(0).get_handle()->getj()]);
        double *b(Adata[get_access(1).get_handle()->geti()*DIM + get_access(1).get_handle()->getj()]);
#ifdef USE_MKL
        double DONE=1.0;
        int nb = NB;
        dtrsm("R", "L", "T", "N", &nb, &nb, &DONE, a, &nb, b, &nb);
#endif
#ifdef USE_ACML
        dtrsm('R', 'L', 'T', 'N', NB, NB, 1.0, a, NB, b, NB);
#endif
    }
    std::string get_name() { return "trsm"; }
};

// ==========================================================================
// Cholesky Decomposition
// ==========================================================================
static void cholesky(const int num_threads, const size_t numBlocks) {

    SuperGlue<Options> sg(num_threads);

    Handle<Options> **A = new Handle<Options>*[numBlocks];
    for (size_t i = 0; i < numBlocks; ++i)
        A[i] = new Handle<Options>[numBlocks];

    for (size_t i = 0; i < numBlocks; ++i)
        for (size_t j = 0; j < numBlocks; ++j)
            A[i][j].set(i, j);

    Time::TimeUnit t1 = Time::getTime();
    for (size_t j = 0; j < numBlocks; j++) {
        for (size_t k = 0; k < j; k++) {
            for (size_t i = j+1; i < numBlocks; i++) {
                // A[i,j] = A[i,j] - A[i,k] * (A[j,k])^t
                sg.submit(new gemm(A[i][k], A[j][k], A[i][j]));
            }
        }
        for (size_t i = 0; i < j; i++) {
            // A[j,j] = A[j,j] - A[j,i] * (A[j,i])^t
            sg.submit(new syrk(A[j][i], A[j][j]));
        }

        // Cholesky Factorization of A[j,j]
        sg.submit(new potrf(A[j][j]));

        for (size_t i = j+1; i < numBlocks; i++) {
            // A[i,j] <- A[i,j] = X * (A[j,j])^t
            sg.submit(new trsm(A[j][j], A[i][j]));
        }
    }

    sg.wait(A[numBlocks-1][numBlocks-1]);

    Time::TimeUnit t2 = Time::getTime();
    std::cout << "#cores=" << sg.get_num_cpus()
              << " #blocks=" << numBlocks << "x" << numBlocks
              << " blocksize=" << NB << "x" << NB
              << " time=" << (t2-t1) << " cycles" << std::endl;
}

static void convert_to_blocks(double *Alin, double **A) {
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++)
            A[(j/NB)*DIM + i/NB][(i%NB)*NB+j%NB] = Alin[i*N+j];
}

void fill_random(double *Alin, int NN) {
    for (int i = 0; i < NN; i++)
        Alin[i] = ((double)rand())/((double)RAND_MAX);
}

// ==========================================================================
// Main
// ==========================================================================
int main(int argc, char *argv[]) {

    if (argc >= 3) {
        NB = (long) atoi(argv[1]);
        DIM = (long) atoi(argv[2]);
    }
    else {
        printf("usage: %s NB DIM [num_cores]\n\n", argv[0]);
        printf("  example: %s 256 16\n", argv[0]);
        exit(0);
    }
    N = NB*DIM;

    int num_workers = -1;
    if (argc >= 4)
        num_workers = atoi(argv[3])-1;

    // fill the matrix with random values
    double *Alin = (double *) malloc(N*N * sizeof(double));
    fill_random(Alin, N*N);

    // make it positive definite
    for (size_t i = 0; i < N; i++)
        Alin[i*N + i] += N;

#ifdef NOT_DEFINED
    double *a(Alin);
    int info = 0;
    int nb = N;

    Time::TimeUnit t1, t2;

#ifdef USE_MKL
    mkl_set_num_threads(num_cores);
{
    mkl_set_dynamic(0);
    t1 = Time::getTime();
    dpotrf("L", &nb, a, &nb, &info);
    t2 = Time::getTime();
}
#endif
#ifdef USE_ACML
    t1 = Time::getTime();
    dpotrf('L', NB, a, NB, &info);
    t2 = Time::getTime();
#endif

    std::cout << NB << " " << DIM << " " << num_cores << " " << t2-t1 << std::endl;
    return 0;
#endif

    // blocked matrix
    Adata = (double **) malloc(DIM*DIM*sizeof(double *));   // A: DIM x DIM
    for (size_t i = 0; i < DIM*DIM; i++)
        Adata[i] = (double *) malloc(NB*NB*sizeof(double)); // A[i][j]: NB x NB

    convert_to_blocks(Alin, Adata);

    std::cerr<<"run " << num_workers+1 << " NB=" << NB << " DIM=" << DIM <<  std::endl;

#ifdef USE_MKL
    mkl_set_num_threads(1);
#endif

    cholesky(num_workers, DIM);

    Options::Instrumentation::dump("execution.log");
    return 0;
}
