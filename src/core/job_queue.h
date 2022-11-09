#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include "api_types.h"

// TODO: Not loving this style of code organization where all these things get their own little
// header. Casey's platform layer design seems nicer.

typedef struct job_queue_t
{
    void *opaque;
} job_queue_t;

job_queue_t create_job_queue(size_t thread_count, size_t queue_size);
void destroy_job_queue(job_queue_t queue);

typedef struct job_context_t
{
    int thread_index;
} job_context_t;

typedef void (*job_t)(job_context_t *context, void *userdata);
void add_job_to_queue(job_queue_t queue, job_t job, void *userdata);
void wait_on_queue(job_queue_t queue);

#endif /* JOB_QUEUE_H */
