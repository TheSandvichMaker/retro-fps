#pragma once

#include "core/api_types.h"

DREAM_GLOBAL size_t query_processor_count(void);

DREAM_GLOBAL bool wait_on_address(volatile void *address, void *compare_address, size_t address_size);
DREAM_GLOBAL void wake_by_address(void *address);
DREAM_GLOBAL void wake_all_by_address(void *address);

DREAM_GLOBAL void mutex_lock           (mutex_t *mutex);
DREAM_GLOBAL void mutex_unlock         (mutex_t *mutex);
DREAM_GLOBAL bool mutex_try_lock       (mutex_t *mutex);
DREAM_GLOBAL void mutex_shared_lock    (mutex_t *mutex);
DREAM_GLOBAL void mutex_shared_unlock  (mutex_t *mutex);
DREAM_GLOBAL bool mutex_shared_try_lock(mutex_t *mutex);

#define mutex_scoped_lock(mutex) DEFER_LOOP(mutex_lock(mutex), mutex_unlock(mutex))

DREAM_GLOBAL void cond_sleep       (cond_t *cond, mutex_t *mutex);
DREAM_GLOBAL void cond_sleep_shared(cond_t *cond, mutex_t *mutex);
DREAM_GLOBAL void cond_wake        (cond_t *cond);
DREAM_GLOBAL void cond_wake_all    (cond_t *cond);

//
//
//

DREAM_GLOBAL void wait_group_add (wait_group_t *group, int64_t delta);
DREAM_GLOBAL void wait_group_done(wait_group_t *group); // TODO: I don't love this name
DREAM_GLOBAL void wait_group_wait(wait_group_t *group);

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
// up to 64 bytes of per-job data (so you don't have to allocate it yourself)
DREAM_GLOBAL void add_job_to_queue_with_data_(job_queue_t queue, job_proc_t proc, void *userdata, size_t userdata_size);
#define add_job_to_queue_with_data(queue, proc, data) add_job_to_queue_with_data_(queue, proc, &(data), sizeof(data))
DREAM_GLOBAL void wait_on_queue(job_queue_t queue);
