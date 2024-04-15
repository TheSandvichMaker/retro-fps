#pragma once

#include "core/api_types.h"

fn size_t query_processor_count(void);

fn bool wait_on_address(volatile void *address, void *compare_address, size_t address_size);
fn void wake_by_address(void *address);
fn void wake_all_by_address(void *address);

fn void mutex_lock           (mutex_t *mutex);
fn void mutex_unlock         (mutex_t *mutex);
fn bool mutex_try_lock       (mutex_t *mutex);
fn void mutex_shared_lock    (mutex_t *mutex);
fn void mutex_shared_unlock  (mutex_t *mutex);
fn bool mutex_shared_try_lock(mutex_t *mutex);

#define mutex_scoped_lock(mutex) DEFER_LOOP(mutex_lock(mutex), mutex_unlock(mutex))

fn void cond_sleep       (cond_t *cond, mutex_t *mutex);
fn void cond_sleep_shared(cond_t *cond, mutex_t *mutex);
fn void cond_wake        (cond_t *cond);
fn void cond_wake_all    (cond_t *cond);

//
//
//

fn void wait_group_add (wait_group_t *group, int64_t delta);
fn void wait_group_done(wait_group_t *group); // TODO: I don't love this name
fn void wait_group_wait(wait_group_t *group);

typedef struct job_queue_t
{
    void *opaque;
} job_queue_t;

fn job_queue_t create_job_queue(size_t thread_count, size_t queue_size);
fn void destroy_job_queue(job_queue_t queue);
fn size_t get_job_queue_thread_count(job_queue_t queue);

typedef struct job_context_t
{
    int thread_index;
} job_context_t;

typedef void (*job_proc_t)(job_context_t *context, void *userdata);
fn void add_job_to_queue(job_queue_t queue, job_proc_t proc, void *userdata);
// up to 64 bytes of per-job data (so you don't have to allocate it yourself)
fn void add_job_to_queue_with_data_(job_queue_t queue, job_proc_t proc, void *userdata, size_t userdata_size);
#define add_job_to_queue_with_data(queue, proc, data) add_job_to_queue_with_data_(queue, proc, &(data), sizeof(data))
fn void wait_on_queue(job_queue_t queue);
