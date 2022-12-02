#ifndef BVH_H
#define BVH_H

#include "core/api_types.h"

typedef struct bvh_node_t
{
    rect3_t  bounds;       // 24
    uint32_t left_first;   // 28
    uint16_t count;        // 30, count > 0 == leaf node
    uint16_t split_axis;   // 32
                           // can fit two per cache line
} bvh_node_t;

typedef struct bvh_brush_t
{
    uint16_t face_count;
    uint32_t face_index;
} bvh_brush_t;

typedef struct bvh_face_poly_t
{
    uint16_t texture_index;
    uint16_t lightmap_index;
    uint16_t vertex_count;
    uint32_t first_vertex;
} bvh_face_poly_t;

typedef struct bvh_face_meta_t
{
    v3_t normal;
    v3_t s, t, lm_s, lm_t;
} bvh_face_meta_t;

typedef struct bvh_texture_info_t
{
    uint32_t width;             // 4
    uint32_t height;            // 8
    uint16_t format;            // 10
    uint16_t stride;            // 12
    uint32_t pixels_offset;     // 16
} bvh_texture_info_t;

typedef struct bvh_t
{
    uint32_t           node_count;
    uint32_t           texture_count;
    uint32_t           face_count;
    uint32_t           indices;
    uint32_t           vertex_count;

    bvh_node_t         *nodes;
    bvh_texture_info_t *texture_infos;
    resource_handle_t  *texture_handles;
    uint32_t           *texture_pixels;
    bvh_face_poly_t    *face_polys;

    uint16_t *indices;

    struct
    {
        v3_t *positions;
        v2_t *texcoords;
        v2_t *lightmap_texcoords;
    } vertex;
} bvh_t;

#endif /* BVH_H */
