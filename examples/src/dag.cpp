#include "sg/superglue.hpp"
#include "sg/option/savedag_task.hpp"
#include "sg/option/savedag_data.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {
    typedef Enable TaskId;
    typedef Enable HandleId;
    typedef Enable TaskName;
    typedef Enable HandleName;
    typedef SaveDAG<Options> LogDAG;
};

struct gemm : public Task<Options> {
    gemm(Handle<Options> &a, Handle<Options> &b, Handle<Options> &c) {
        register_access(ReadWriteAdd::read, a);
        register_access(ReadWriteAdd::read, b);
        register_access(ReadWriteAdd::add, c);
    }
    void run() {}
    std::string get_name() { return "gemm"; }
};
struct syrk : public Task<Options> {
    syrk(Handle<Options> &a, Handle<Options> &b) {
        register_access(ReadWriteAdd::read, a);
        register_access(ReadWriteAdd::add, b);
    }
    void run() {}
    std::string get_name() { return "syrk"; }
};
struct potrf : public Task<Options> {
    potrf(Handle<Options> &a) {
        register_access(ReadWriteAdd::write, a);
    }
    void run() {}
    std::string get_name() { return "potrf"; }
};
struct trsm : public Task<Options> {
    trsm(Handle<Options> &a, Handle<Options> &b) {
        register_access(ReadWriteAdd::read, a);
        register_access(ReadWriteAdd::write, b);
    }
    void run() {}
    std::string get_name() { return "trsm"; }
};

int main() {

    const size_t numBlocks = 3;

    Handle<Options> **A = new Handle<Options>*[numBlocks];
    for (size_t i = 0; i < numBlocks; ++i) {
        A[i] = new Handle<Options>[numBlocks];
        for (size_t j = 0; j < numBlocks; ++j) {
            std::stringstream ss;
            ss<<"("<<i<<","<<j<<")";
            A[i][j].set_name(ss.str().c_str());
        }
    }

    SuperGlue<Options> sg(1);

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
    sg.barrier();
    SaveDAG_task<Options>::dump("cholesky.dot");
    SaveDAG_data<Options>::dump("cholesky_data.dot");
    return 0;
}
