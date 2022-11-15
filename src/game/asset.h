#ifndef ASSET_H
#define ASSET_H

#include "core/api_types.h"

typedef struct image_t
{
    uint32_t w, h, pitch;
    uint32_t *pixels;
} image_t;

typedef struct cubemap_t
{
    uint32_t w, h, pitch;
    uint32_t *pixels[6];
} cubemap_t;

image_t load_image(arena_t *arena, string_t path);
bool split_image_into_cubemap(const image_t *source, cubemap_t *cubemap);

#endif /* ASSET_H */
