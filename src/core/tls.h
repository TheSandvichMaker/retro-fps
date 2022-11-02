#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include "api_types.h"

typedef struct thread_context_t
{
    jmp_buf jmp; // allows you to have a "global" jump, but per thread so it's thread-safe
    arena_t temp;
} thread_context_t;

#ifdef _MSC_VER
#define thread_local __declspec(thread)
#else
#define thread_local _Thread_local
#endif

extern thread_local thread_context_t __thread_context;
#define THREAD_CONTEXT (&__thread_context)

#define temp (&THREAD_CONTEXT->temp)
#define reset_temp() m_reset_and_decommit(temp)

#endif /* THREAD_CONTEXT_H */
