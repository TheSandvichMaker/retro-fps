#ifndef DREAM_POOL_H
#define DREAM_POOL_H

#include "api_types.h"

enum { BD_FREE_BIT = 1 << 31 };

typedef struct bulk_item_t
{
    uint32_t generation;
} bulk_item_t;

typedef struct free_item_t
{
    uint32_t generation;
    uint32_t next;
} free_item_t;

typedef enum bulk_flags_t
{
	BULK_FLAGS_CONCURRENT = 0x1,
} bulk_flags_t;

typedef struct pool_t
{
    size_t watermark;
    uint32_t item_size;
    uint32_t align;
    char *buffer;
	uint32_t flags;
	mutex_t lock;
} pool_t;

resource_handle_t pool_get_handle(pool_t *pool, void *item);

void *pool_add(pool_t *pool);
resource_handle_t pool_add_item(pool_t *pool, const void *item);
void *pool_get(pool_t *pool, resource_handle_t handle);
bool  pool_rem(pool_t *pool, resource_handle_t handle);
bool  pool_rem_item(pool_t *pool, void *item);

#define INIT_POOL(Type)           { .item_size = sizeof(Type), .align = alignof(Type) }
#define INIT_POOL_EX(Type, Flags) { .item_size = sizeof(Type), .align = alignof(Type), .flags = Flags }

typedef struct pool_iter_t
{
    pool_t *pool;
    void *data;
} pool_iter_t;

pool_iter_t pool_iter(pool_t *pool);
bool pool_iter_valid(pool_iter_t *it);
void pool_iter_next(pool_iter_t *it);
void pool_iter_rem(pool_iter_t *it);

#endif /* DREAM_POOL_H */
