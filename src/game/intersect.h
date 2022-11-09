#ifndef INTERSECT_H
#define INTERSECT_H

#include "core/api_types.h"

float ray_intersect_rect3(v3_t o, v3_t d, rect3_t rect);
float ray_intersect_triangle(v3_t o, v3_t d, v3_t a, v3_t b, v3_t c, v3_t *uvw);

v3_t get_normal_rect3(v3_t hit_p, rect3_t rect);

typedef struct intersect_result_t
{
    float t;                             // hit t

    struct map_brush_t *brush;           // hit brush
    struct map_poly_t  *poly;            // hit poly

    v3_t uvw;                            // barycentrics of hit triangle
    uint32_t triangle_offset;            // offset into poly indices array for triangle
} intersect_result_t;

typedef struct intersect_params_t
{
    v3_t o;                              // ray origin
    v3_t d;                              // ray direction

    bool occlusion_test;                 // early outs on the first hit if true, does not fill in any hit information

    float min_t, max_t;                  // min and max hit distance (default min_t: 0.001, default max_t: FLT_MAX)

    size_t ignore_brush_count;          
    struct map_brush_t **ignore_brushes; // brushes to ignore during intersection
} intersect_params_t;

bool intersect_map(struct map_t *map, const intersect_params_t *params, intersect_result_t *result);

#endif /* INTERSECT_H */
