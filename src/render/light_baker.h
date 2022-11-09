#ifndef LIGHT_BAKER_H
#define LIGHT_BAKER_H

#include "core/api_types.h"

typedef struct bake_light_params_t
{
    arena_t *arena;
    struct map_t *map;

    int ray_count;      // number of diffuse rays per lightmap pixel
    int ray_recursion;  // maximum recursion depth for indirect lighting

    v3_t sun_direction;
    v3_t sun_color;
    v3_t ambient_color;
} bake_light_params_t;

typedef struct bake_light_debug_ray_t
{
    struct bake_light_debug_ray_t *next;

    struct map_brush_t *spawn_brush;

    v3_t o;
    v3_t d;
    float t;
} bake_light_debug_ray_t;

typedef struct bake_light_debug_ray_list_t
{
    bake_light_debug_ray_t *first;
    bake_light_debug_ray_t *last;
} bake_light_debug_ray_list_t;

typedef struct bake_light_debug_data_t
{
    bake_light_debug_ray_list_t   direct_rays;
    bake_light_debug_ray_list_t indirect_rays;
} bake_light_debug_data_t;

typedef struct bake_light_results_t
{
    bake_light_debug_data_t debug_data;
} bake_light_results_t;

void bake_lighting(const bake_light_params_t *params, bake_light_results_t *results);

#endif /* LIGHT_BAKER_H */
