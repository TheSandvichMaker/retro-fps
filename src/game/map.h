// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#if DEBUG
#define LIGHTMAP_SCALE 8
#else
#define LIGHTMAP_SCALE 4
#endif

typedef struct collision_geometry_t collision_geometry_t;

typedef uint32_t map_surface_flags_t;

typedef uint32_t map_content_flags_t;
typedef enum map_content_flags_enum_t
{
	MapContentFlag_water,
} map_content_flags_enum_t;

typedef struct map_plane_t
{
    v3_t a, b, c;
    string_t texture;
    v4_t s, t;
    float rot, scale_x, scale_y;

    v3_t lm_origin;
    v3_t lm_s, lm_t;

    float lm_scale_x, lm_scale_y;
    int   lm_tex_w,   lm_tex_h;

	map_content_flags_t content_flags;
	map_surface_flags_t surface_flags;
} map_plane_t;

typedef struct map_coplanar_surface_t
{
	uint32_t plane_poly_count;
	uint32_t first_plane_poly;
} map_coplanar_surface_t;

typedef struct map_poly_t
{
    uint32_t index_count;
    uint32_t first_index;

    uint32_t vertex_count;
    uint32_t first_vertex;

    v3_t normal;
	float distance;

    mesh_handle_t    mesh;
    asset_hash_t     texture;
    texture_handle_t lightmap;
	rhi_texture_t    lightmap_rhi;
} map_poly_t;

typedef struct map_lightmap_t
{
	uint32_t poly_count;
	uint32_t first_lightmap_poly_index;

	v3_t origin;
	v3_t s, t;
	float scale_x, scale_y;
} map_lightmap_t;

typedef struct map_poly_hull_t
{
	uint32_t index_count;
} map_poly_hull_t;

typedef struct map_property_t
{
    string_t key;
    string_t val;
} map_property_t;

typedef struct map_brush_t
{
    uint32_t plane_poly_count;
    uint32_t first_plane_poly;

    rect3_t bounds;
} map_brush_t;

typedef struct map_point_light_t
{
    v3_t p;
    v3_t color;
} map_point_light_t;

typedef struct map_entity_t
{
    uint32_t brush_count;
    uint32_t first_brush_edge; // go through the brush_edges array to find your brushes.

    uint32_t property_count;
    uint32_t first_property;
} map_entity_t;

typedef struct map_bvh_node_t 
{
    rect3_t bounds;
    uint32_t left_first; // points to brushes
    uint16_t count;
    uint16_t split_axis;
} map_bvh_node_t;

typedef struct map_t
{
    rect3_t bounds;

	struct lum_bake_state_t *lightmap_state;
    uint32_t fogmap_w;
    uint32_t fogmap_h;
    uint32_t fogmap_d;
    texture_handle_t fogmap;

    v3_t fogmap_offset;
    v3_t fogmap_dim;
    float fog_absorption;
    float fog_density;
    float fog_scattering;
    float fog_phase_k;

    uint32_t node_count;
    uint32_t entity_count;
    uint32_t property_count;
    uint32_t brush_count;
    uint32_t plane_count;
    uint32_t poly_count;
	uint32_t edge_count;
    uint32_t index_count;
    uint32_t vertex_count;
    uint32_t light_count;

    map_bvh_node_t    *nodes;
    map_entity_t      *entities;
    map_property_t    *properties;
    map_brush_t       *brushes;
    uint32_t          *brush_edges;
    map_plane_t       *planes;
    map_poly_t        *polys;
	// map_edge_t        *edges;
    map_point_light_t *lights;

	// collision_geometry_t *collision_player;
    collision_geometry_t collision;

    uint16_t *indices;
    struct
    {
        v3_t *positions;
        v2_t *texcoords;
        v2_t *lightmap_texcoords;
    } vertex;
} map_t;

map_t *load_map(arena_t *arena, string_t path);

bool     is_class      (map_t *map, map_entity_t *entity, string_t classname);
string_t value_from_key(map_t *map, map_entity_t *entity, string_t key);
int      int_from_key  (map_t *map, map_entity_t *entity, string_t key);
float    float_from_key(map_t *map, map_entity_t *entity, string_t key);
v3_t     v3_from_key   (map_t *map, map_entity_t *entity, string_t key);
