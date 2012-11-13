#include "superglue.hpp"
#include <iostream>

struct Options : public DefaultOptions<Options> {
    typedef Enable Logging_DAG;
    typedef Enable TaskId;
    typedef Enable HandleId;
    typedef Enable TaskName;
    typedef Enable HandleName;
};

struct gemm : public Task<Options> {
    gemm(Handle<Options> &a, Handle<Options> &b, Handle<Options> &c) {
        registerAccess(ReadWriteAdd::read, &a);
        registerAccess(ReadWriteAdd::read, &b);
        registerAccess(ReadWriteAdd::add, &c);
    }
    void run() {}
    std::string getName() { return "gemm"; }
};
struct syrk : public Task<Options> {
    syrk(Handle<Options> &a, Handle<Options> &b) {
        registerAccess(ReadWriteAdd::read, &a);
        registerAccess(ReadWriteAdd::add, &b);
    }
    void run() {}
    std::string getName() { return "syrk"; }
};
struct potrf : public Task<Options> {
    potrf(Handle<Options> &a) {
        registerAccess(ReadWriteAdd::write, &a);
    }
    void run() {}
    std::string getName() { return "potrf"; }
};
struct trsm : public Task<Options> {
    trsm(Handle<Options> &a, Handle<Options> &b) {
        registerAccess(ReadWriteAdd::read, &a);
        registerAccess(ReadWriteAdd::write, &b);
    }
    void run() {}
    std::string getName() { return "trsm"; }
};

int main() {

    const size_t numBlocks = 4;

    Handle<Options> **A = new Handle<Options>*[numBlocks];
    for (size_t i = 0; i < numBlocks; ++i)
        A[i] = new Handle<Options>[numBlocks];

    ThreadManager<Options> tm(0);

    for (size_t j = 0; j < numBlocks; j++) {
        for (size_t k = 0; k < j; k++) {
            for (size_t i = j+1; i < numBlocks; i++) {
                // A[i,j] = A[i,j] - A[i,k] * (A[j,k])^t
                tm.submit(new gemm(A[i][k], A[j][k], A[i][j]));
            }
        }
        for (size_t i = 0; i < j; i++) {
            // A[j,j] = A[j,j] - A[j,i] * (A[j,i])^t
            tm.submit(new syrk(A[j][i], A[j][j]));
        }

        // Cholesky Factorization of A[j,j]
        tm.submit(new potrf(A[j][j]));

        for (size_t i = j+1; i < numBlocks; i++) {
            // A[i,j] <- A[i,j] = X * (A[j,j])^t
            tm.submit(new trsm(A[j][j], A[i][j]));
        }
    }
    tm.barrier();
    Log<Options>::dump_dag("cholesky.dot");
    return 0;
}
