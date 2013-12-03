#include "csuperglue.h"
#include <stdlib.h>
#include <stdio.h>

struct gemm_data { double *A, *B, *C; };
void gemm(void *args) { /* fprintf(stderr, "gemm\n"); */ }

struct syrk_data { double *A, *B; };
void syrk(void *args) { /* fprintf(stderr, "syrk\n"); */ }

struct potrf_data { double *A; };
void potrf(void *args) { /* fprintf(stderr, "potrf\n"); */ }

struct trsm_data { double *A, *B; };
void trsm(void *args) { /* fprintf(stderr, "trsm\n"); */ }

int main() {

    int i, j, k;
    const size_t n = 30;

    sg_handle_t *h = sg_create_handles(n*n);
    double **A = malloc(n*n * sizeof(double *));

    /* fill A with pointers to blocks here... */

    sg_init();

    for (j = 0; j < n; j++) {
        sg_task_t task;
        struct potrf_data potrf_args;

        for (k = 0; k < j; k++) {
            for (i = j+1; i < n; i++) {
                /* A[i,j] = A[i,j] - A[i,k] * (A[j,k])^t */
                struct gemm_data args;
                args.A = A[i*n+k];
                args.B = A[j*n+k];
                args.C = A[i*n+j];
                task = sg_create_task(gemm, &args, sizeof(args), "gemm");
                sg_register_access(task, sg_read, h[i*n+k]);
                sg_register_access(task, sg_read, h[j*n+k]);
                sg_register_access(task, sg_add, h[i*n+j]);
                sg_submit_task(task);
            }
        }
        for (i = 0; i < j; i++) {
            /* A[j,j] = A[j,j] - A[j,i] * (A[j,i])^t */
            struct syrk_data args;
            args.A = A[j*n+i];
            args.B = A[j*n+j];
            task = sg_create_task(syrk, &args, sizeof(args), "syrk");
            sg_register_access(task, sg_read, h[j*n+i]);
            sg_register_access(task, sg_add, h[j*n+j]);
            sg_submit_task(task);
        }

        /* Cholesky Factorization of A[j,j] */
        potrf_args.A = A[j*n+j];
        task = sg_create_task(potrf, &potrf_args, sizeof(potrf_args), "potrf");
        sg_register_access(task, sg_write, h[j*n+j]);
        sg_submit_task(task);

        for (i = j+1; i < n; i++) {
            /* A[i,j] <- A[i,j] = X * (A[j,j])^t */
            struct trsm_data args;
            args.A = A[j*n+j];
            args.B = A[i*n+j];
            task = sg_create_task(trsm, &args, sizeof(args), "trsm");
            sg_register_access(task, sg_read, h[j*n+j]);
            sg_register_access(task, sg_write, h[i*n+j]);
            sg_submit_task(task);
        }
    }
    sg_barrier();
    sg_write_log("execution2.log");
    sg_destroy();

    free(A);
    sg_destroy_handles(h, n*n);

    return 0;
}
