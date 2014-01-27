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
    sg_init();
    sg_submit_inplace(my_task, &i, 0, NULL);
    sg_barrier();
    sg_submit_inplace(my_task, &i, 0, NULL);
    sg_destroy();
    return 0;
}
