#ifndef BULK_DATA_H
#define BULK_DATA_H

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

typedef struct bulk_t
{
    size_t watermark;
    uint32_t item_size;
    uint32_t align;
    char *buffer;
} bulk_t;

resource_handle_t bd_get_handle(bulk_t *bd, void *item);

void *bd_add(bulk_t *bd);
resource_handle_t bd_add_item(bulk_t *bd, const void *item);
void *bd_get(bulk_t *bd, resource_handle_t handle);
bool  bd_rem(bulk_t *bd, resource_handle_t handle);
bool  bd_rem_item(bulk_t *bd, void *item);

#define INIT_BULK_DATA(type) { .item_size = sizeof(type), .align = alignof(type) }

typedef struct bd_iter_t
{
    bulk_t *bd;
    void *data;
} bd_iter_t;

bd_iter_t bd_iter(bulk_t *bd);
bool bd_iter_valid(bd_iter_t *it);
void bd_iter_next(bd_iter_t *it);
void bd_iter_rem(bd_iter_t *it);

#endif /* BULK_DATA_H */
