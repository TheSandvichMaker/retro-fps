#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//
//
//

#include "core/thread.h"
#include "core/atomics.h"
#include "core/arena.h"

bool wait_on_address(volatile void *address, void *compare_address, size_t address_size)
{
	return WaitOnAddress(address, compare_address, address_size, INFINITE);
}

void wake_by_address(void *address)
{
	WakeByAddressSingle(address);
}

void wake_all_by_address(void *address)
{
	WakeByAddressAll(address);
}

//
// mutex
//

void mutex_lock(mutex_t *mutex)
{
	SRWLOCK *srw_lock = (SRWLOCK *)mutex;
	AcquireSRWLockExclusive(srw_lock);
}

void mutex_unlock(mutex_t *mutex)
{
	SRWLOCK *srw_lock = (SRWLOCK *)mutex;
	ReleaseSRWLockExclusive(srw_lock);
}

bool mutex_try_lock(mutex_t *mutex)
{
	SRWLOCK *srw_lock = (SRWLOCK *)mutex;
	return TryAcquireSRWLockExclusive(srw_lock);
}

void mutex_shared_lock(mutex_t *mutex)
{
	SRWLOCK *srw_lock = (SRWLOCK *)mutex;
	AcquireSRWLockShared(srw_lock);
}

void mutex_shared_unlock(mutex_t *mutex)
{
	SRWLOCK *srw_lock = (SRWLOCK *)mutex;
	ReleaseSRWLockShared(srw_lock);
}

bool mutex_try_shared_lock(mutex_t *mutex)
{
	SRWLOCK *srw_lock = (SRWLOCK *)mutex;
	return TryAcquireSRWLockShared(srw_lock);
}

//
// condition variable
//

void cond_sleep(cond_t *cond, mutex_t *mutex)
{
	CONDITION_VARIABLE *win32_cond  = (CONDITION_VARIABLE *)cond;
	SRWLOCK            *win32_mutex = (SRWLOCK *)mutex;
	SleepConditionVariableSRW(win32_cond, win32_mutex, INFINITE, 0);
}

void cond_sleep_shared(cond_t *cond, mutex_t *mutex)
{
	CONDITION_VARIABLE *win32_cond  = (CONDITION_VARIABLE *)cond;
	SRWLOCK            *win32_mutex = (SRWLOCK *)mutex;
	SleepConditionVariableSRW(win32_cond, win32_mutex, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
}

void cond_wake(cond_t *cond)
{
	CONDITION_VARIABLE *win32_cond = (CONDITION_VARIABLE *)cond;
	WakeConditionVariable(win32_cond);
}

void cond_wake_all(cond_t *cond)
{
	CONDITION_VARIABLE *win32_cond = (CONDITION_VARIABLE *)cond;
	WakeAllConditionVariable(win32_cond);
}

//
//
//

size_t query_processor_count(void)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	// boldly presume hyperthreading
	size_t actual_processor_count = info.dwNumberOfProcessors / 2;
	return actual_processor_count;
}

typedef struct job_thread_t
{
    HANDLE handle;
} job_thread_t;

typedef struct job_queue_entry_t
{
    job_proc_t proc;
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

typedef struct job_proc_params_t
{
    job_queue_internal_t *queue;
    int thread_index;
} job_proc_params_t;

static DWORD WINAPI job_queue_thread_proc(void *userdata)
{
    job_proc_params_t *params = userdata;

    job_queue_internal_t *queue = params->queue;

    job_context_t context = {
        .thread_index = params->thread_index,
    };

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
                entry->proc(&context, entry->userdata);

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

    // just keep these around who cares
    job_proc_params_t *params = m_alloc_array(&queue->arena, thread_count, job_proc_params_t);

    for (size_t thread_index = 0; thread_index < thread_count; thread_index++)
    {
        job_proc_params_t *param = &params[thread_index];
        param->queue        = queue;
        param->thread_index = (int)thread_index;

        job_thread_t *thread = &queue->threads[thread_index];
        thread->handle = CreateThread(NULL, 0, job_queue_thread_proc, param, 0, NULL);
    }

    job_queue_t result = { queue };
    return result;
}

size_t get_job_queue_thread_count(job_queue_t handle)
{
    job_queue_internal_t *queue = handle.opaque;
	return queue->thread_count;
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

void add_job_to_queue(job_queue_t handle, job_proc_t proc, void *userdata)
{
    job_queue_internal_t *queue = handle.opaque;

    uint32_t write      = queue->next_write;
    uint32_t next_write = write + 1;

    job_queue_entry_t *entry = &queue->entries[write % queue->queue_size];
    entry->proc     = proc;
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
