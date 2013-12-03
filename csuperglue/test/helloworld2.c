#include "csuperglue.h"
#include <stdio.h>

void my_task(void *args) {
    printf("Hello world!\n");
}

int main() {
    sg_task_t task;
    sg_init();
    task = sg_create_task(my_task, NULL, 0, NULL);
    sg_submit_task(task);
    sg_destroy();
    return 0;
}
