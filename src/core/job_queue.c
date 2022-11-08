#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//
//
//

#include "job_queue.h"
#include "core/core.h"

typedef struct job_thread_t
{
    HANDLE handle;
} job_thread_t;

typedef struct job_queue_entry_t
{
    job_t job;
    void *userdata;
} job_queue_entry_t;

typedef struct job_queue_internal_t
{
    arena_t arena;

    size_t thread_count;
    job_thread_t *threads;

    size_t queue_size;
    job_queue_entry_t *entries;

    volatile uint32_t next_read;
    volatile uint32_t next_write;
    volatile uint32_t jobs_in_flight;

    HANDLE event_stop;
    HANDLE event_done;

    HANDLE semaphore;
} job_queue_internal_t;

static DWORD WINAPI job_queue_thread_proc(void *userdata)
{
    job_queue_internal_t *queue = userdata;

    for (;;)
    {
        uint32_t entry_index = queue->next_read;
        if (entry_index != queue->next_write)
        {
            uint32_t next_entry_index = entry_index + 1;
            uint32_t exchanged_index = InterlockedCompareExchange((volatile long *)&queue->next_read, next_entry_index, entry_index);

            if (exchanged_index == entry_index)
            {
                job_queue_entry_t *entry = &queue->entries[entry_index % queue->queue_size];
                entry->job(entry->userdata);

                uint32_t jobs_count = InterlockedDecrement((volatile long *)&queue->jobs_in_flight);

                // TODO: Looks questionable to me. Is this right?
                if (jobs_count == 0)
                    SetEvent(queue->event_done);
            }
        }
        else
        {
            HANDLE handles[] = { queue->event_stop, queue->semaphore };

            if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0)
                break;
        }
    }

    return 0;
}

job_queue_t create_job_queue(size_t thread_count, size_t queue_size)
{
    job_queue_internal_t *queue = m_bootstrap(job_queue_internal_t, arena);

    queue->thread_count = thread_count;
    queue->threads = m_alloc_array(&queue->arena, thread_count, job_thread_t);

    queue->queue_size = queue_size;
    queue->entries = m_alloc_array(&queue->arena, queue_size, job_queue_entry_t);

    queue->event_stop  = CreateEventA(NULL, TRUE, FALSE, NULL);
    queue->event_done  = CreateEventA(NULL, TRUE, TRUE, NULL);

    queue->semaphore = CreateSemaphoreA(NULL, 0, (LONG)thread_count, NULL);

    for (size_t thread_index = 0; thread_index < thread_count; thread_index++)
    {
        job_thread_t *thread = &queue->threads[thread_index];
        thread->handle = CreateThread(NULL, 0, job_queue_thread_proc, queue, 0, NULL);
    }

    job_queue_t result = { queue };
    return result;
}

void destroy_job_queue(job_queue_t handle)
{
    job_queue_internal_t *queue = handle.opaque;

    SetEvent(queue->event_stop);
    for (size_t thread_index = 0; thread_index < queue->thread_count; thread_index++)
    {
        job_thread_t *thread = &queue->threads[thread_index];
        WaitForSingleObject(thread->handle, INFINITE);
        CloseHandle(thread->handle);
    }
    CloseHandle(queue->event_stop);
    CloseHandle(queue->event_done);
    CloseHandle(queue->semaphore);

    m_release(&queue->arena);
}

void add_job_to_queue(job_queue_t handle, job_t job, void *userdata)
{
    job_queue_internal_t *queue = handle.opaque;

    uint32_t write      = queue->next_write;
    uint32_t next_write = write + 1;

    job_queue_entry_t *entry = &queue->entries[write % queue->queue_size];
    entry->job      = job;
    entry->userdata = userdata;

    MemoryBarrier();

    queue->next_write = next_write;

    InterlockedIncrement((volatile long *)&queue->jobs_in_flight);
    ResetEvent(queue->event_done);

    ReleaseSemaphore(queue->semaphore, 1, NULL);
}

void wait_on_queue(job_queue_t handle)
{
    job_queue_internal_t *queue = handle.opaque;

    if (queue->jobs_in_flight > 0)
        WaitForSingleObject(queue->event_done, INFINITE);
}
