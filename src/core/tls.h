// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct thread_context_t
{
    jmp_buf jmp; // allows you to have a "global" jump, but per thread so it's thread-safe
} thread_context_t;

#ifdef _MSC_VER
#define thread_local __declspec(thread)
#else
#define thread_local _Thread_local
#endif

extern thread_local thread_context_t __thread_context;
#define THREAD_CONTEXT (&__thread_context)
