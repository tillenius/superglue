#include "csuperglue.h"
#include <stdio.h>

struct my_args {
    int i;
};

void my_task(void *args_) {
    struct my_args *args = (struct my_args *) args_;
    /* args here is a private copy */
    printf("Hello world! Private copy of args = %d\n", args->i);
    ++args->i;
}

int main() {
    struct my_args args;
    sg_task_t task;

    args.i = 32;

    sg_init();
    task = sg_create_task(my_task, &args, sizeof(struct my_args), NULL);
    sg_submit_task(task);
    sg_barrier();
    task = sg_create_task(my_task, &args, sizeof(struct my_args), NULL);
    sg_submit_task(task);
    sg_destroy();
    return 0;
}
