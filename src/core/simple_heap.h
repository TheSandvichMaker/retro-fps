#pragma once

// NOTE: To avoid abuse, the simple heap allocator's max single allocation size is limited to 16K

typedef struct simple_heap_block_header_t
{
	struct simple_heap_block_header_t *next;
	struct simple_heap_block_header_t *prev;
	// TODO: add debug build only metadata to verify correct freeing
} simple_heap_block_header_t;

typedef struct simple_heap_t
{
	arena_t *arena;
	simple_heap_block_header_t free_block_lists[10];
} simple_heap_t;

fn void  simple_heap_init        (simple_heap_t *heap, arena_t *arena);
fn void *simple_heap_alloc_nozero(simple_heap_t *heap, uint16_t size);
fn void *simple_heap_alloc       (simple_heap_t *heap, uint16_t size);
fn void  simple_heap_free        (simple_heap_t *heap, void *pointer, uint16_t size); // if you lie about the size, you're in trouble

#define simple_heap_alloc_struct(heap, type) \
	(type *)simple_heap_alloc(heap, sizeof(type))

#define simple_heap_alloc_struct_nozero(heap, type) \
	(type *)simple_heap_alloc_nozero(heap, sizeof(type))

#define simple_heap_free_struct(heap, pointer) \
	simple_heap_free(heap, pointer, sizeof(*(pointer)))
