#include "core.h"

void m_init_with_memory(arena_t *arena, void *memory, size_t size)
{
    ASSERT(arena->buffer == NULL);
    
    arena->buffer    = memory;
    arena->at        = arena->buffer;
    arena->end       = arena->buffer + size;
    arena->committed = arena->end;

    arena->owns_memory = false;
}

void *m_alloc_nozero(arena_t *arena, size_t size, size_t align)
{
    if (!arena->buffer)
    {
        arena->buffer    = vm_reserve(NULL, ARENA_CAPACITY);
        arena->at        = arena->buffer;
        arena->end       = arena->buffer + ARENA_CAPACITY;
        arena->committed = arena->buffer;

        arena->owns_memory = true;
    }

    if (ALWAYS(m_size_remaining_for_align(arena, align) >= size))
    {
        char *result = align_address(arena->at, align);
        if (result + size > arena->committed)
        {
            size_t to_commit = align_forward(result + size - arena->committed, ARENA_COMMIT_CHUNK_SIZE);
            vm_commit(arena->committed, to_commit);

            arena->committed += to_commit;
        }

        arena->at = result + size;
        return result;
    }

    return NULL;
}

void *m_alloc(arena_t *arena, size_t size, size_t align)
{
    void *result = m_alloc_nozero(arena, size, align);
    zero_memory(result, size);
    return result;
}

void *m_copy(arena_t *arena, const void *src, size_t size)
{
    void *result = m_alloc(arena, size, 16);
    copy_memory(result, src, size);
    return result;
}

char *m_alloc_string(arena_t *arena, size_t size)
{
    return m_alloc(arena, size, DEFAULT_STRING_ALIGN);
}

wchar_t *m_alloc_string16(arena_t *arena, size_t size)
{
    return m_alloc(arena, sizeof(uint16_t)*size, DEFAULT_STRING_ALIGN);
}

size_t m_size_used(const arena_t *arena)
{
    return arena->at - arena->buffer;
}

size_t m_size_remaining(const arena_t *arena)
{
    if (!arena->buffer)
    {
        return ARENA_CAPACITY;
    }
    return arena->end - arena->at;
}

size_t m_size_remaining_for_align(const arena_t *arena, size_t align)
{
    size_t result = m_size_remaining(arena);
    result = align_backward(result, align);
    return result;
}

void m_pre_commit(arena_t *arena, size_t size)
{
    if (arena->at + size > arena->end)
    {
        size = arena->end - arena->at;
    }

    size_t to_commit = align_forward(arena->at + size - arena->committed, ARENA_COMMIT_CHUNK_SIZE);
    vm_commit(arena->committed, to_commit);

    arena->committed += to_commit;
}

void m_reset(arena_t *arena)
{
    arena->at = arena->buffer;
}

void m_reset_and_decommit(arena_t *arena)
{
    if (arena->owns_memory)
    {
        char  *decommit_from  = arena->buffer + ARENA_DEFAULT_COMMIT_PRESERVE_THRESHOLD;
        size_t decommit_bytes = MAX(0, arena->committed - decommit_from);

		if (decommit_bytes)
			vm_decommit(decommit_from, decommit_bytes);

        arena->at        = arena->buffer;
        arena->committed = arena->buffer;
    }
    else
    {
        m_reset(arena);
    }
}

void m_release(arena_t *arena)
{
    if (arena->owns_memory)
    {
        void *buffer = arena->buffer;
        zero_struct(arena);
        vm_release(buffer);
    }
    else
    {
        m_reset(arena);
    }
}

arena_marker_t m_get_marker(arena_t *arena)
{
    arena_marker_t result;
    result.arena = arena;
    result.at    = arena->at;
    return result;
}

void m_reset_to_marker(arena_t *arena, arena_marker_t marker)
{
    ASSERT(marker.arena == arena);

    if (marker.at == NULL)
        marker.at = arena->buffer;

    ASSERT(marker.at >= arena->buffer && marker.at < arena->end);

    arena->at = marker.at;
}

void *m_bootstrap_(size_t size, size_t align, size_t arena_offset)
{
    arena_t arena = { 0 };
    void *result = m_alloc(&arena, size, align);
    copy_memory((char *)result + arena_offset, &arena, sizeof(arena));
    return result;
}
