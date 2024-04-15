#pragma once

#include "engine/api_types.h"

typedef struct job_context_t
{
    int thread_index;
} job_context_t;

#define JOB_PROC(name) void name(job_context_t *context, void *userdata)
typedef JOB_PROC((*job_proc_t));

// wait group can be null
fn void add_job(job_proc_t proc, void *userdata, wait_group_t *wait_group);

// up to 64 bytes of per-job data (so you don't have to allocate it yourself)
fn void add_job_with_data_(job_proc_t proc, void *userdata, size_t userdata_size, wait_group_t *wait_group);
#define add_job_with_data(proc, data, wait_group) add_job_with_data_(proc, &(data), sizeof(data), wait_group)

