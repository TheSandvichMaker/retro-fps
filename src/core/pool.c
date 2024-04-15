#include "core/core.h"

//
// pool
//

fn_local pool_item_t *pool_item_from_item(void *item)
{
    return ((pool_item_t *)item) - 1;
}

fn_local void *item_from_pool_item(pool_item_t *pool_item)
{
    return pool_item + 1;
}

fn_local size_t pool_stride(pool_t *pool)
{
    return align_forward(MAX(sizeof(free_item_t), pool->item_size) + sizeof(pool_item_t), pool->align);
}

fn_local uint32_t index_from_pool_item(pool_t *pool, pool_item_t *pool_item)
{
    return (uint32_t)(((char *)pool_item - pool->buffer) / pool_stride(pool));
}

fn_local void *pool_item_at_index(pool_t *pool, size_t index)
{
    return (void *)(pool->buffer + index*pool_stride(pool));
}

fn_local void pool_init(pool_t *pool)
{
    ASSERT_MSG(pool->item_size, "POOL INITIALIZATION FAILURE: ITEM SIZE IS ZERO"); // this needs to be initialized by the user

	if (pool->flags & POOL_FLAGS_CONCURRENT) mutex_lock(&pool->lock);

    if (ALWAYS(!pool->buffer))
    {
        pool->buffer = vm_reserve(NULL, POOL_RESERVE_SIZE);
        vm_commit(pool->buffer, POOL_COMMIT_SIZE);

        pool->watermark = pool_stride(pool); // first entry is the freelist sentinel
    }

	if (pool->flags & POOL_FLAGS_CONCURRENT) mutex_unlock(&pool->lock);
}

void *pool_add(pool_t *pool)
{
    if (!pool->buffer)  pool_init(pool);

    void *result = NULL;

	if (pool->flags & POOL_FLAGS_CONCURRENT) mutex_lock(&pool->lock);

    free_item_t *sentinel = pool_item_at_index(pool, 0);
    if (sentinel->next != 0)
    {
        free_item_t *free_item = pool_item_at_index(pool, sentinel->next);
        sentinel->next = free_item->next;

        free_item->generation &= ~POOL_FREE_BIT;

        pool_item_t *pool_item = (pool_item_t *)free_item;

        result = item_from_pool_item(pool_item);
        zero_memory(result, pool->item_size);
    }
    else
    {
        size_t stride = pool_stride(pool);

        size_t new_size  = pool->watermark + stride;
        bool   new_chunk = (align_forward(new_size, POOL_COMMIT_SIZE) > align_forward(pool->watermark, POOL_COMMIT_SIZE));

        if (new_chunk)
            vm_commit(pool->buffer + align_backward(new_size, POOL_COMMIT_SIZE), POOL_COMMIT_SIZE);

        size_t index = pool->watermark / stride;

        // bump the size
        pool->watermark += stride;

        pool_item_t *pool_item = pool_item_at_index(pool, index);

        // don't need to zero the memory because it's fresh from VirtualAlloc, guaranteed to be zero
        result = item_from_pool_item(pool_item);
    }

	pool->count += 1;

	if (pool->flags & POOL_FLAGS_CONCURRENT) mutex_unlock(&pool->lock);

    return result;
}

resource_handle_t pool_add_item(pool_t *pool, const void *item)
{
    void *result = pool_add(pool);
    copy_memory(result, item, pool->item_size);
    return pool_get_handle(pool, result);
}

void *pool_get_impl(pool_t *pool, resource_handle_t handle)
{
    if (NEVER(!pool->buffer))  FATAL_ERROR("Pool not initialized!");

    void *result = NULL;

    size_t stride = pool_stride(pool);
    size_t count  = pool->watermark / stride;

    if (handle.index > 0 && ALWAYS(handle.index < count))
    {
        pool_item_t *pool_item = pool_item_at_index(pool, handle.index);
        if (pool_item->generation == handle.generation)
        {
            result = item_from_pool_item(pool_item);
        }
    }

    return result;
}

bool pool_rem(pool_t *pool, resource_handle_t handle)
{
    if (NEVER(!pool->buffer))  FATAL_ERROR("Pool not initialized!");

    bool result = false;

	if (pool->flags & POOL_FLAGS_CONCURRENT) mutex_lock(&pool->lock);

    size_t stride = pool_stride(pool);
    size_t count  = pool->watermark / stride;

    if (handle.index > 0 && ALWAYS(handle.index < count))
    {
        free_item_t *sentinel  = pool_item_at_index(pool, 0);
        free_item_t *free_item = pool_item_at_index(pool, handle.index);

        free_item->generation += 1;
        free_item->generation |= POOL_FREE_BIT;

        free_item->next = sentinel->next;
        sentinel ->next = handle.index;

        result = true;

		pool->count -= 1;
    }

	if (pool->flags & POOL_FLAGS_CONCURRENT) mutex_unlock(&pool->lock);

    return result;
}

bool pool_rem_item(pool_t *pool, void *item)
{
    resource_handle_t handle = pool_get_handle(pool, item);
    return pool_rem(pool, handle);
}

resource_handle_t pool_get_handle(pool_t *pool, void *item)
{
    if (NEVER(!pool->buffer))  return NULL_RESOURCE_HANDLE;

    pool_item_t *pool_item = pool_item_from_item(item);

    if (NEVER((((uintptr_t)pool_item) & (pool->align-1)) != 0))  return NULL_RESOURCE_HANDLE;

    uint32_t index = index_from_pool_item(pool, pool_item);

    size_t count = pool->watermark / pool_stride(pool);
    if (NEVER(index <  0))      return NULL_RESOURCE_HANDLE;
    if (NEVER(index >= count))  return NULL_RESOURCE_HANDLE;

    resource_handle_t handle = {
        .index      = index,
        .generation = pool_item->generation,
    };

    return handle;
}

//
// pool_iter_t
//

pool_iter_t pool_iter(pool_t *pool)
{
    pool_iter_t it = {
        .pool   = pool,
        .data = item_from_pool_item((pool_item_t *)pool->buffer),
    };
    pool_iter_next(&it);
    return it;
}

bool pool_iter_valid(pool_iter_t *it)
{
    return !!it->data;
}

void pool_iter_next(pool_iter_t *it)
{
    if (!it->data)
        return;

    for (;;)
    {
        it->data = (char *)it->data + pool_stride(it->pool);

        pool_item_t *pool_item = pool_item_from_item(it->data);

        if ((char *)pool_item >= it->pool->buffer + it->pool->watermark)
        {
            it->data = NULL;
            break;
        }

        if (!(pool_item->generation & POOL_FREE_BIT))
            break;
    }
}

void pool_iter_rem(pool_iter_t *it)
{
	pool_rem_item(it->pool, it->data);
}
