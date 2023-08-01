#ifndef DREAMCORE_THREAD_H
#define DREAMCORE_THREAD_H

#include "core/api_types.h"

DREAM_GLOBAL bool wait_on_address(volatile void *address, void *compare_address, size_t address_size);
DREAM_GLOBAL void wake_by_address(void *address);
DREAM_GLOBAL void wake_all_by_address(void *address);

DREAM_GLOBAL void mutex_lock           (mutex_t *mutex);
DREAM_GLOBAL void mutex_unlock         (mutex_t *mutex);
DREAM_GLOBAL bool mutex_try_lock       (mutex_t *mutex);
DREAM_GLOBAL void mutex_shared_lock    (mutex_t *mutex);
DREAM_GLOBAL void mutex_shared_unlock  (mutex_t *mutex);
DREAM_GLOBAL bool mutex_shared_try_lock(mutex_t *mutex);

DREAM_GLOBAL void cond_sleep       (cond_t *cond, mutex_t *mutex);
DREAM_GLOBAL void cond_sleep_shared(cond_t *cond, mutex_t *mutex);
DREAM_GLOBAL void cond_wake        (cond_t *cond);
DREAM_GLOBAL void cond_wake_all    (cond_t *cond);

DREAM_GLOBAL size_t query_processor_count(void);

typedef struct job_queue_t
{
    void *opaque;
} job_queue_t;

DREAM_GLOBAL job_queue_t create_job_queue(size_t thread_count, size_t queue_size);
DREAM_GLOBAL void destroy_job_queue(job_queue_t queue);
DREAM_GLOBAL size_t get_job_queue_thread_count(job_queue_t queue);

typedef struct job_context_t
{
    int thread_index;
} job_context_t;

typedef void (*job_proc_t)(job_context_t *context, void *userdata);
DREAM_GLOBAL void add_job_to_queue(job_queue_t queue, job_proc_t proc, void *userdata);
DREAM_GLOBAL void wait_on_queue(job_queue_t queue);

#endif
