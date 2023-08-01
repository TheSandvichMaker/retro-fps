#ifndef DREAMCORE_THREAD_H
#define DREAMCORE_THREAD_H

#include "core/api_types.h"

DREAM_API bool wait_on_address(volatile void *address, void *compare_address, size_t address_size);
DREAM_API void wake_by_address(void *address);
DREAM_API void wake_all_by_address(void *address);

DREAM_API void mutex_lock           (mutex_t *mutex);
DREAM_API void mutex_unlock         (mutex_t *mutex);
DREAM_API bool mutex_try_lock       (mutex_t *mutex);
DREAM_API void mutex_shared_lock    (mutex_t *mutex);
DREAM_API void mutex_shared_unlock  (mutex_t *mutex);
DREAM_API bool mutex_shared_try_lock(mutex_t *mutex);

DREAM_API void cond_sleep       (cond_t *cond, mutex_t *mutex);
DREAM_API void cond_sleep_shared(cond_t *cond, mutex_t *mutex);
DREAM_API void cond_wake        (cond_t *cond);
DREAM_API void cond_wake_all    (cond_t *cond);

DREAM_API size_t query_processor_count(void);

typedef struct job_queue_t
{
    void *opaque;
} job_queue_t;

DREAM_API job_queue_t create_job_queue(size_t thread_count, size_t queue_size);
DREAM_API void destroy_job_queue(job_queue_t queue);
DREAM_API size_t get_job_queue_thread_count(job_queue_t queue);

typedef struct job_context_t
{
    int thread_index;
} job_context_t;

typedef void (*job_proc_t)(job_context_t *context, void *userdata);
DREAM_API void add_job_to_queue(job_queue_t queue, job_proc_t proc, void *userdata);
DREAM_API void wait_on_queue(job_queue_t queue);

#endif
