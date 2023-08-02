#ifndef DREAM_POOL_H
#define DREAM_POOL_H

#include "api_types.h"

#define POOL_RESERVE_SIZE GB(16)
#define POOL_COMMIT_SIZE  KB(16)

enum { POOL_FREE_BIT = 1 << 31 };

typedef struct pool_item_t
{
    uint32_t generation;
} pool_item_t;

typedef struct free_item_t
{
    uint32_t generation;
    uint32_t next;
} free_item_t;

typedef enum pool_flags_t
{
	POOL_FLAGS_CONCURRENT = 0x1,
} pool_flags_t;

typedef struct pool_t
{
    size_t   watermark;
	size_t   count;
    uint32_t item_size;
    uint16_t align;
	uint16_t flags;
    char    *buffer;
	mutex_t  lock;
} pool_t;

DREAM_GLOBAL void             *pool_add       (pool_t *pool);
DREAM_GLOBAL resource_handle_t pool_add_item  (pool_t *pool, const void *item);
DREAM_GLOBAL void             *pool_get       (pool_t *pool, resource_handle_t handle);
DREAM_GLOBAL bool              pool_rem       (pool_t *pool, resource_handle_t handle);
DREAM_GLOBAL bool              pool_rem_item  (pool_t *pool, void *item);
DREAM_GLOBAL resource_handle_t pool_get_handle(pool_t *pool, void *item);

#define INIT_POOL(Type)           { .item_size = sizeof(Type), .align = alignof(Type) }
#define INIT_POOL_EX(Type, Flags) { .item_size = sizeof(Type), .align = alignof(Type), .flags = Flags }

typedef struct pool_iter_t
{
    pool_t *pool;
    void *data;
} pool_iter_t;

DREAM_GLOBAL pool_iter_t pool_iter(pool_t *pool);
DREAM_GLOBAL bool pool_iter_valid (pool_iter_t *it);
DREAM_GLOBAL void pool_iter_next  (pool_iter_t *it);
DREAM_GLOBAL void pool_iter_rem   (pool_iter_t *it);

#endif /* DREAM_POOL_H */
