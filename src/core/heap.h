// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct heap_t
{
    int dummy;
} heap_t;

void *heap_alloc(heap_t *heap, size_t size, size_t align);
void *heap_free (heap_t *heap, size_t size, void *ptr);
