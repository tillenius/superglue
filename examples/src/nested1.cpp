#include "sg/superglue.hpp"
#include "sg/option/instr_trace.hpp"

struct Options : public DefaultOptions<Options> {
    typedef Trace<Options> Instrumentation;
    typedef Enable PassTaskExecutor;
};

double *A;
Handle<Options> *handles;
const int DIM = 8;

Handle<Options> &get_handle(int i, int j, int block_size) {
    int dim(DIM);
    int offset(0);

    while (block_size > 1) {
        offset += dim*dim;
        block_size /= 2;
        dim /= 2;
        i /= 2;
        j /= 2;
    }
    return handles[offset + i*dim+j];
}

struct nested_task : public Task<Options> {
    int ii, jj, bsz;
    nested_task(int i_, int j_, int bsz_) : ii(i_), jj(j_), bsz(bsz_) {
        register_access(ReadWriteAdd::write, get_handle(ii, jj, bsz));
    }

    void run(TaskExecutor<Options> &te) {
        if (bsz == 1) {
            A[ii*DIM+jj] += 1.0;
            return;
        }

        const int num_blocks = 2;
        const int nbsz = bsz/num_blocks;

        for (int i = 0; i < num_blocks; ++i)
            for (int j = 0; j < num_blocks; ++j)
                te.submit(new nested_task(ii+i*nbsz, jj+j*nbsz, nbsz));
    }
};


int main() {
    A = new double[DIM*DIM];
    handles = new Handle<Options>[8*8 + 4*4 + 2*2 + 1];

    assert( &get_handle(0,0,1) == handles );
    assert( &get_handle(7,7,1)+1 == &get_handle(0,0,2) );
    assert( &get_handle(6,6,2)+1 == &get_handle(0,0,4) );
    assert( &get_handle(4,4,4)+1 == &get_handle(0,0,8) );


    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 8; ++j)
            A[i*8+j] = 0;

    SuperGlue<Options> sg;
    sg.submit(new nested_task(0, 0, 8));
    sg.barrier();

    double sum = 0.0;
    double max = 0.0;

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (A[i*8+j] > max)
                max = A[i*8+j];
            sum += A[i*8+j];
        }
    }

    fprintf(stderr, "sum = %f , max = %f\n", sum, max);

    Options::Instrumentation::dump("trace.log");
    return 0;
}
