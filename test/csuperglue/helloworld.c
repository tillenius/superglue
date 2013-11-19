#include "csuperglue.h"
#include <stdio.h>

void my_task(void *args) {
    printf("Hello world!\n");
}

int main() {
    sg_init();
    sg_submit(my_task, 0, 0, 0, 0);
    sg_destroy();
    return 0;
}
