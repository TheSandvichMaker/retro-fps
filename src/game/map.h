#ifndef MAP_H
#define MAP_H

#include "core/core.h"
#include "render/render.h"

#define LIGHTMAP_SCALE 1

typedef struct map_plane_t
{
    struct map_plane_t *next;

    v3_t a, b, c;
    string_t texture;
    v4_t s, t;
    float rot, scale_x, scale_y;

    v2_t tex_mins;
    v2_t tex_maxs;
    v3_t lm_origin;

    unsigned poly_index;
} map_plane_t;

typedef struct map_poly_t
{
    uint32_t vertex_index;
    uint32_t vertex_count;
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

typedef struct map_t
{
    map_entity_t *first_entity;
    vertex_brush_t *vertices;
} map_t;

// returns first entity in list
map_entity_t *parse_map(arena_t *arena, string_t path);
map_t *load_map(arena_t *arena, string_t path);

bool is_class(map_entity_t *entity, string_t classname);
string_t value_from_key(map_entity_t *entity, string_t key);
int int_from_key(map_entity_t *entity, string_t key);
v3_t v3_from_key(map_entity_t *entity, string_t key);

#endif /* MAP_H */
