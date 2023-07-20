#include "core/core.h"

#define BULK_DATA_RESERVE_SIZE GB(16)
#define BULK_DATA_COMMIT_SIZE  KB(16)

static bulk_item_t *bulk_item_from_item(void *item)
{
    return ((bulk_item_t *)item) - 1;
}

static void *item_from_bulk_item(bulk_item_t *bulk_item)
{
    return bulk_item + 1;
}

static size_t bd_stride(bulk_t *bd)
{
    return align_forward(MAX(sizeof(free_item_t), bd->item_size) + sizeof(bulk_item_t), bd->align);
}

static uint32_t index_from_bulk_item(bulk_t *bd, bulk_item_t *bulk_item)
{
    return (uint32_t)(((char *)bulk_item - bd->buffer) / bd_stride(bd));
}

static void *bd_at_index(bulk_t *bd, size_t index)
{
    return (void *)(bd->buffer + index*bd_stride(bd));
}

static void bd_init(bulk_t *bd)
{
    ASSERT_MSG(bd->item_size, "BULK DATA INITIALIZATION FAILURE: ITEM SIZE IS ZERO"); // this needs to be initialized by the user

	if (bd->flags & BULK_FLAGS_CONCURRENT) mutex_lock(bd->lock);

    if (ALWAYS(!bd->buffer))
    {
        bd->buffer = vm_reserve(NULL, BULK_DATA_RESERVE_SIZE);
        vm_commit(bd->buffer, BULK_DATA_COMMIT_SIZE);

        bd->watermark = bd_stride(bd); // first entry is the freelist sentinel
    }

	if (bd->flags & BULK_FLAGS_CONCURRENT) mutex_unlock(bd->lock);
}

void *bd_add(bulk_t *bd)
{
    if (!bd->buffer)  bd_init(bd);

    void *result = NULL;

	if (bd->flags & BULK_FLAGS_CONCURRENT) mutex_lock(bd->lock);

    free_item_t *sentinel = bd_at_index(bd, 0);
    if (sentinel->next != 0)
    {
        free_item_t *free_item = bd_at_index(bd, sentinel->next);
        sentinel->next = free_item->next;

        free_item->generation &= ~BD_FREE_BIT;

        bulk_item_t *bulk_item = (bulk_item_t *)free_item;

        result = item_from_bulk_item(bulk_item);
        zero_memory(result, bd->item_size);
    }
    else
    {
        size_t stride = bd_stride(bd);

        size_t new_size = bd->watermark + stride;

        bool new_chunk = (align_forward(new_size, BULK_DATA_COMMIT_SIZE) > align_forward(bd->watermark, BULK_DATA_COMMIT_SIZE));

        if (new_chunk)
            vm_commit(bd->buffer + align_backward(new_size, BULK_DATA_COMMIT_SIZE), BULK_DATA_COMMIT_SIZE);

        size_t index = bd->watermark / stride;

        // bump the size
        bd->watermark += stride;

        bulk_item_t *bulk_item = bd_at_index(bd, index);

        // don't need to zero the memory because it's fresh from VirtualAlloc, guaranteed to be zero
        result = item_from_bulk_item(bulk_item);
    }

	if (bd->flags & BULK_FLAGS_CONCURRENT) mutex_unlock(bd->lock);

    return result;
}

resource_handle_t bd_add_item(bulk_t *bd, const void *item)
{
    void *result = bd_add(bd);
    copy_memory(result, item, bd->item_size);
    return bd_get_handle(bd, result);
}

void *bd_get(bulk_t *bd, resource_handle_t handle)
{
    if (NEVER(!bd->buffer))  FATAL_ERROR("Bulk data not initialized!");

    void *result = NULL;

    size_t stride = bd_stride(bd);
    size_t count  = bd->watermark / stride;

    if (handle.index > 0 && ALWAYS(handle.index < count))
    {
        bulk_item_t *bulk_item = bd_at_index(bd, handle.index);
        if (bulk_item->generation == handle.generation)
        {
            result = item_from_bulk_item(bulk_item);
        }
    }

    return result;
}

bool bd_rem(bulk_t *bd, resource_handle_t handle)
{
    if (NEVER(!bd->buffer))  FATAL_ERROR("Bulk data not initialized!");

    bool result = false;

	if (bd->flags & BULK_FLAGS_CONCURRENT) mutex_lock(bd->lock);

    size_t stride = bd_stride(bd);
    size_t count  = bd->watermark / stride;

    if (handle.index > 0 && ALWAYS(handle.index < count))
    {
        free_item_t *sentinel  = bd_at_index(bd, 0);
        free_item_t *free_item = bd_at_index(bd, handle.index);

        free_item->generation += 1;
        free_item->generation |= BD_FREE_BIT;

        free_item->next = sentinel->next;
        sentinel ->next = handle.index;

        result = true;
    }

	if (bd->flags & BULK_FLAGS_CONCURRENT) mutex_unlock(bd->lock);

    return result;
}

bool bd_rem_item(bulk_t *bd, void *item)
{
    resource_handle_t handle = bd_get_handle(bd, item);
    return bd_rem(bd, handle);
}

resource_handle_t bd_get_handle(bulk_t *bd, void *item)
{
    if (NEVER(!bd->buffer))  return NULL_RESOURCE_HANDLE;

    bulk_item_t *bulk_item = bulk_item_from_item(item);

    if ((((uintptr_t)bulk_item) & (bd->align-1)) != 0)
        FATAL_ERROR("Funny bulk data item align!");

    size_t count = bd->watermark / bd_stride(bd);

    uint32_t index = index_from_bulk_item(bd, bulk_item);

    if (NEVER(index < 0))      return NULL_RESOURCE_HANDLE;
    if (NEVER(index > count))  return NULL_RESOURCE_HANDLE;

    resource_handle_t handle = {
        .index      = index,
        .generation = bulk_item->generation,
    };

    return handle;
}

bd_iter_t bd_iter(bulk_t *bd)
{
    bd_iter_t it = {
        .bd   = bd,
        .data = item_from_bulk_item((bulk_item_t *)bd->buffer),
    };
    bd_iter_next(&it);
    return it;
}

bool bd_iter_valid(bd_iter_t *it)
{
    return !!it->data;
}

void bd_iter_next(bd_iter_t *it)
{
    if (!it->data)
        return;

    for (;;)
    {
        it->data = (char *)it->data + bd_stride(it->bd);

        bulk_item_t *bulk_item = bulk_item_from_item(it->data);

        if ((char *)bulk_item >= it->bd->buffer + it->bd->watermark)
        {
            it->data = NULL;
            break;
        }

        if (!(bulk_item->generation & BD_FREE_BIT))
            break;
    }
}

void bd_iter_rem(bd_iter_t *it)
{
	bd_rem_item(it->bd, it->data);
}
