#ifndef ARENA_H
#define ARENA_H

#include "api_types.h"

// initializes arena with pre-allocated memory. this is optional, if you default-initialize an arena it will be backed by 16 GiB of virtual memory
void m_init_with_memory(arena_t *arena, void *memory, size_t size);

#define ARENA_CAPACITY (16ull << 30)
#define ARENA_COMMIT_CHUNK_SIZE (16ull << 10)
#define ARENA_DEFAULT_COMMIT_PRESERVE_THRESHOLD ARENA_COMMIT_CHUNK_SIZE

void *m_alloc(arena_t *arena, size_t size, size_t align);
void *m_alloc_nozero(arena_t *arena, size_t size, size_t align);
void *m_copy(arena_t *arena, const void *src, size_t size);

// adheres to string alignment rules
char    *m_alloc_string  (arena_t *arena, size_t size); 
wchar_t *m_alloc_string16(arena_t *arena, size_t size); 

#define m_alloc_struct(arena, type)       (type *)m_alloc(arena,         sizeof(type), alignof(type))
#define m_alloc_array(arena, count, type) (type *)m_alloc(arena, (count)*sizeof(type), alignof(type))

#define m_alloc_struct_nozero(arena, type)       (type *)m_alloc_nozero(arena,         sizeof(type), alignof(type))
#define m_alloc_array_nozero(arena, count, type) (type *)m_alloc_nozero(arena, (count)*sizeof(type), alignof(type))

#define m_copy_struct(arena, src)       m_copy(arena, src, sizeof(*(src)))
#define m_copy_array(arena, src, count) m_copy(arena, src, (count)*sizeof(*(src)))

size_t m_size_used(const arena_t *arena);
size_t m_size_remaining(const arena_t *arena);
size_t m_size_remaining_for_align(const arena_t *arena, size_t align);

void m_pre_commit(arena_t *arena, size_t size);

void m_reset(arena_t *arena);
void m_reset_and_decommit(arena_t *arena);
void m_release(arena_t *arena);

arena_marker_t m_get_marker(arena_t *arena);
void m_reset_to_marker(arena_t *arena, arena_marker_t marker);

#define m_scoped(ARENA) for (arena_marker_t __marker = m_get_marker(ARENA); __marker.arena; m_reset_to_marker(ARENA, __marker), __marker.arena = NULL)

void *m_bootstrap_(size_t size, size_t align, size_t arena_offset);
#define m_bootstrap(type, arena) m_bootstrap_(sizeof(type), alignof(type), offsetof(type, arena))

#endif /* ARENA_H */
