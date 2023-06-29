#ifndef DREAMCORE_THREAD_H
#define DREAMCORE_THREAD_H

#include "core/api_types.h"

// TODO: Not loving this style of code organization where all these things get their own little
// header. Casey's platform layer design seems nicer.

typedef struct rw_mutex_t
{
	void *opaque; // legal to be zero-initialized
} rw_mutex_t;

DREAM_API void rw_mutex_lock           (rw_mutex_t mutex);
DREAM_API void rw_mutex_unlock         (rw_mutex_t mutex);
DREAM_API bool rw_mutex_try_lock       (rw_mutex_t mutex);
DREAM_API void rw_mutex_shared_lock    (rw_mutex_t mutex);
DREAM_API void rw_mutex_shared_unlock  (rw_mutex_t mutex);
DREAM_API bool rw_mutex_shared_try_lock(rw_mutex_t mutex);

DREAM_API size_t query_processor_count(void);

typedef struct job_queue_t
{
    void *opaque;
} job_queue_t;

DREAM_API job_queue_t create_job_queue(size_t thread_count, size_t queue_size);
DREAM_API void destroy_job_queue(job_queue_t queue);

typedef struct job_context_t
{
    int thread_index;
} job_context_t;

typedef void (*job_proc_t)(job_context_t *context, void *userdata);
DREAM_API void add_job_to_queue(job_queue_t queue, job_proc_t proc, void *userdata);
DREAM_API void wait_on_queue(job_queue_t queue);

#endif
