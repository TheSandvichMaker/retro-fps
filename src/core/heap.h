#ifndef HEAP_H
#define HEAP_H

typedef struct heap_t
{
    int dummy;
} heap_t;

void *heap_alloc(heap_t *heap, size_t size, size_t align);
void *heap_free (heap_t *heap, size_t size, void *ptr);

#endif /* HEAP_H */
