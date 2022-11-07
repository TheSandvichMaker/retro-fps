#ifndef ASSET_H
#define ASSET_H

#include "core/api_types.h"

static inline uint32_t pack_color(v4_t color)
{
    color.x = CLAMP(color.x, 0.0f, 1.0f);
    color.y = CLAMP(color.y, 0.0f, 1.0f);
    color.z = CLAMP(color.z, 0.0f, 1.0f);
    color.w = CLAMP(color.w, 0.0f, 1.0f);

    uint32_t result = (((uint32_t)(255.0f*color.x) <<  0) |
                       ((uint32_t)(255.0f*color.y) <<  8) |
                       ((uint32_t)(255.0f*color.z) << 16) |
                       ((uint32_t)(255.0f*color.w) << 24));
    return result;
}

static inline v4_t unpack_color(uint32_t color)
{
    float rcp_255 = 1.0f / 255.0f;

    v4_t result;
    result.x = rcp_255*(float)((color >>  0) & 0xFF);
    result.y = rcp_255*(float)((color >>  8) & 0xFF);
    result.z = rcp_255*(float)((color >> 16) & 0xFF);
    result.w = rcp_255*(float)((color >> 24) & 0xFF);
    return result;
}

typedef struct image_t
{
    uint32_t w, h, pitch;
    uint32_t *pixels;
} image_t;

image_t load_image(arena_t *arena, string_t path);
bool split_image_into_cubemap_faces(image_t source, image_t *faces);

#endif /* ASSET_H */
