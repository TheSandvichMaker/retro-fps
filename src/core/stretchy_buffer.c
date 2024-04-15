// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

void *sb__alloc(arena_t *arena, unsigned capacity, size_t item_size)
{

    sb_t *sb = m_alloc(arena, sizeof(sb_t) + capacity*item_size, 16);
    sb->arena    = arena;
    sb->count    = 0;
    sb->capacity = capacity;
    return sb__data(sb);
}

void sb__ensure_space(void **sb_at, unsigned count, size_t item_size)
{
    if (*sb_at)
    {
        sb_t *sb = sb__header(*sb_at);

        if (sb->count + count > sb->capacity)
        {
            unsigned new_capacity = MAX(sb->count + count, sb->capacity*2);
            sb_t *new_sb = sb__header(sb__alloc(sb->arena, new_capacity, item_size));

            new_sb->count = sb->count;

            memcpy(sb__data(new_sb), sb__data(sb), sb->count*item_size);

            *sb_at = sb__data(new_sb);
        }
    }
    else
    {
        *sb_at = sb__alloc(m_get_temp(NULL, 0), MAX(8, count), item_size);
    }
}
