#ifndef STRETCHY_BUFFER_H
#define STRETCHY_BUFFER_H

#include "api_types.h"

typedef struct sb_t
{
    arena_t *arena;
    unsigned count;
    unsigned capacity;
} sb_t;

#define stretchy_buffer(type) type *

#define sb_init(arena, type) sb__alloc(arena, 8, sizeof(type))
#define sb_add(sb) (sb__ensure_space((void **)&(sb), 1, sizeof(*sb)), &sb[sb__header(sb)->count++])
#define sb_push(sb, item) (sb__ensure_space((void **)&(sb), 1, sizeof(*sb)), sb[sb__header(sb)->count++] = item)
#define sb_arena(sb) (sb ? sb__header(sb)->arena : NULL)
#define sb_count(sb) (sb ? sb__header(sb)->count : 0)
#define sb_capacity(sb) (sb ? sb__header(sb)->capacity : 0)
#define sb_copy(arena, sb) (m_copy(sb_arena(sb), sb, sb_count(sb)*sizeof(*sb)))
#define sb_remove_unordered(sb, index) (sb ? (sb)[index] = (sb)[--sb__header(sb)->count] : 0)

//
//
//

#define sb__data(header) (void *)(((sb_t *)(header)) + 1)
#define sb__header(sb) (((sb_t *)(sb)) - 1)

void *sb__alloc(arena_t *arena, unsigned capacity, size_t item_size);
void sb__ensure_space(void **sb, unsigned count, size_t item_size);

#endif /* STRETCHY_BUFFER_H */
