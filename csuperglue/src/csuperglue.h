#ifndef SG_CSUPERGLUE_H_INCLUDED
#define SG_CSUPERGLUE_H_INCLUDED

#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *sg_handle_t;
typedef void *sg_task_t;
typedef void (*sg_task_function)(void *);
enum sg_access_type { sg_read = 1, sg_add, sg_write };

/* Initialize SuperGlue. Starts worker threads. */
void sg_init();

/* Initialize SuperGlue. Starts worker threads, but do not run any tasks yet. */
void sg_init_paused();
/* Start running tasks. (If initialized via sg_init_paused()) */
void sg_execute();

/* Shut down SuperGlue */
void sg_destroy();

/* Create a task. The task must be submitted to SuperGlue using sg_submit_task, or there is a memory leak. */
sg_task_t sg_create_task(sg_task_function function, void *args, size_t argsize, const char *name);
/* Version that does not make a private copy of the arguments. */
sg_task_t sg_create_inplace_task(sg_task_function function, void *args, const char *name);

/* Add an access to a task. */
void sg_register_access(sg_task_t task, enum sg_access_type type, sg_handle_t handle);

/* Submit a task to SuperGlue. SuperGlue takes ownership of the task. */
void sg_submit_task(sg_task_t task);

/* Create and submit a task to SuperGlue.
     function -- function with signature "void my_function(void *args)"
     args     -- user-defined arguments to be passed to the function
     argsize  -- size of user-defined arguments
     name     -- name of task, for logging (ignored otherwise)
     deps     -- null-terminated list of access types and handles:
                 Example: [sg_access_type, sg_handle, sg_access_type, sg_handle, 0]
*/
void sg_submit(sg_task_function function, void *args, size_t argsize, const char *name, ...);
/* Version that does not make a private copy of the arguments. */
void sg_submit_inplace(sg_task_function function, void *args, const char *name, ...);

/* Wait for all submitted tasks to finish */
void sg_barrier();

/* Create an array of handles */
sg_handle_t *sg_create_handles(int num);

/* Destroy handles */
void sg_destroy_handles(sg_handle_t *handles, int num);

/* Return current time (time stamp counter) */
unsigned long long sg_get_time();

/* Write log-file (if logging is enabled) */
void sg_write_log(const char *filename);

/* Add a log entry (if logging is enabled) */
void sg_log(const char *name, unsigned long long start, unsigned long long stop);

#ifdef __cplusplus
}
#endif

#endif /* SG_CSUPERGLUE_H_INCLUDED */
