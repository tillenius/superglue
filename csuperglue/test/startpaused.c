#include "csuperglue.h"
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

pthread_mutex_t mutex;
int counter = 0;

void my_task(void *args) {
    pthread_mutex_lock(&mutex);
    ++counter;
    pthread_mutex_unlock(&mutex);
}

int main() {
    const int num_tasks = 1000;
    int i;

    pthread_mutex_init(&mutex, NULL);

    /* Start SuperGlue paused: No tasks can run yet. */
    sg_init_paused();

    /* Submit tasks. Will not be run yet. */
    for (i = 0; i < num_tasks; ++i)
        sg_submit(my_task, 0, 0, 0, 0);

    /* Make sure no tasks have run. */
    assert(counter == 0);

    /* Allow tasks to run. */
    sg_execute();

    /* Wait for all tasks to finish. */
    sg_barrier();

    /* Make sure all tasks finished. */
    assert(counter == num_tasks);

    /* Shut down SuperGlue. */
    sg_destroy();

    pthread_mutex_destroy(&mutex);

    return 0;
}
