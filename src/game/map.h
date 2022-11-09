#ifndef MAP_H
#define MAP_H

#include "core/core.h"
#include "render/render.h"
#include "game/asset.h"

#if DEBUG
#define LIGHTMAP_SCALE 8
#else
#define LIGHTMAP_SCALE 2
#endif

typedef struct map_plane_t
{
    struct map_plane_t *next;

    v3_t a, b, c;
    string_t texture;
    v4_t s, t;
    float rot, scale_x, scale_y;

    v2_t lm_tex_mins;
    v2_t lm_tex_maxs;
    v3_t lm_origin;
    v3_t lm_s, lm_t;

    unsigned poly_index;
} map_plane_t;

typedef struct map_poly_t
{
    uint32_t index_count;
    uint32_t vertex_count;
    uint16_t       *indices;
    vertex_brush_t *vertices;

    v3_t normal;

    image_t texture_cpu;

    resource_handle_t mesh;
    resource_handle_t texture;
    resource_handle_t lightmap;
} map_poly_t;

typedef struct map_property_t
{
    struct map_property_t *next;
    string_t key;
    string_t val;
} map_property_t;

typedef struct map_brush_t
{
    struct map_brush_t *next;
    map_plane_t *first_plane, *last_plane;

    size_t poly_count;
    map_poly_t *polys;

    rect3_t bounds;
} map_brush_t;

typedef struct map_entity_t
{
    struct map_entity_t *next;

    map_property_t *first_property, *last_property;
    map_brush_t *first_brush, *last_brush;
} map_entity_t;

typedef struct map_bvh_node_t 
{
    rect3_t bounds;
    uint32_t left_first;
    uint16_t count;
    uint16_t split_axis;
} map_bvh_node_t;

typedef struct map_t
{
    map_entity_t *first_entity;

    uint32_t node_count;
    uint32_t brush_count;

    map_bvh_node_t *nodes;
    map_brush_t **brushes;
} map_t;

// returns first entity in list
map_entity_t *parse_map(arena_t *arena, string_t path);
map_t *load_map(arena_t *arena, string_t path);

bool is_class(map_entity_t *entity, string_t classname);
string_t value_from_key(map_entity_t *entity, string_t key);
int int_from_key(map_entity_t *entity, string_t key);
v3_t v3_from_key(map_entity_t *entity, string_t key);

#endif /* MAP_H */
