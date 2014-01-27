#include "csuperglue.h"
#include <stdio.h>

void my_task(void *args) {
    int *i = (int *) args;
    /* here i is no copy, but points to i in the stack of main() */
    printf("Hello world! Shared copy of args = %d\n", *i);
    ++(*i);
}

int main() {
    int i = 0;
    sg_task_t task;

    sg_init();
    task = sg_create_inplace_task(my_task, &i, NULL);
    sg_submit_task(task);
    sg_barrier();

    task = sg_create_inplace_task(my_task, &i, NULL);
    sg_submit_task(task);
    sg_destroy();
    return 0;
}
