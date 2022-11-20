#ifndef LIGHT_BAKER_H
#define LIGHT_BAKER_H

#include "core/api_types.h"

typedef struct lum_params_t
{
    arena_t *arena;

    struct map_t *map;

    int ray_count;      // number of diffuse rays per lightmap pixel
    int ray_recursion;  // maximum recursion depth for indirect lighting

    int fogmap_cluster_size;
    int fogmap_scale;
    int fog_light_sample_count;
    float fog_base_scattering;

    struct image_t *skybox;

    v3_t sun_direction;
    v3_t sun_color;
    v3_t sky_color;
} lum_params_t;

typedef struct lum_light_sample_t
{
    float shadow_ray_t;

    v3_t contribution;
    v3_t d;
} lum_light_sample_t;

typedef struct lum_path_vertex_t
{
    struct lum_path_vertex_t *next;
    struct lum_path_vertex_t *prev;

    struct map_brush_t *brush;
    struct map_poly_t *poly;

    lum_light_sample_t *light_samples;

    v3_t contribution;
    v3_t throughput;
    v3_t o;

    unsigned light_sample_count;
} lum_path_vertex_t;

typedef struct lum_path_t
{
    struct lum_path_t *next;

    v2i_t source_pixel;

    v3_t contribution;

    uint32_t vertex_count;
    lum_path_vertex_t *first_vertex;
    lum_path_vertex_t * last_vertex;
} lum_path_t;

// TODO: Lightmap texture debug info to map
// from pixel to path easily
//
// TODO: Switch to flat arrays of data for
// debug data and map_t data, so that I can
// refer to stuff by index instead of having
// a pointer rat's nest, which also makes it
// trivial to map between data.
//
// unsigned poly_to_debug_texture_map[...];
// unsigned debug_texture_to_poly_map[...];

typedef struct lum_debug_data_t
{
    lum_path_t *first_path;
    lum_path_t *last_path;
} lum_debug_data_t;

typedef struct lum_results_t
{
    lum_debug_data_t debug_data;
} lum_results_t;

void bake_lighting(const lum_params_t *params, lum_results_t *results);

#endif /* LIGHT_BAKER_H */
