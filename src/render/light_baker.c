#include <stdlib.h>

#include "light_baker.h"
#include "render.h"
#include "diagram.h"
#include "game/map.h"
#include "game/asset.h"
#include "game/intersect.h"
#include "core/core.h"

static uint32_t pack_color(v4_t color)
{
    color.x = CLAMP(color.x, 0.0f, 1.0f);
    color.y = CLAMP(color.x, 0.0f, 1.0f);
    color.z = CLAMP(color.x, 0.0f, 1.0f);
    color.w = CLAMP(color.x, 0.0f, 1.0f);

    uint32_t result = (((uint32_t)(255.0f*color.x) <<  0) |
                       ((uint32_t)(255.0f*color.y) <<  8) |
                       ((uint32_t)(255.0f*color.z) << 16) |
                       ((uint32_t)(255.0f*color.w) << 24));
    return result;
}

typedef struct intersect_result_t
{
    float t;
    map_brush_t *brush;
    map_plane_t *plane;
} intersect_result_t;

typedef struct intersect_params_t
{
    v3_t o;
    v3_t d;

    float min_t, max_t;

    size_t ignore_brush_count;
    map_brush_t **ignore_brushes;
} intersect_params_t;

static bool intersect_map(map_t *map, const intersect_params_t *params, intersect_result_t *result)
{
    float min_t = params->min_t;
    float max_t = params->max_t;

    if (min_t == 0.0f) 
        min_t = 0.001f;

    if (max_t == 0.0f)
        max_t = FLT_MAX;

    float t = max_t;

    v3_t o = params->o;
    v3_t d = params->d;

    map_brush_t *hit_brush = NULL;
    map_plane_t *hit_plane = NULL;

    for (map_entity_t *e = map->first_entity; e; e = e->next)
    {
        for (map_brush_t *brush = e->first_brush; brush; brush = brush->next)
        {
            bool ignore = false;
            for (size_t ignore_brush_index = 0; ignore_brush_index < params->ignore_brush_count; ignore_brush_index++)
            {
                if (brush == params->ignore_brushes[ignore_brush_index])
                {
                    ignore = true;
                    break;
                }
            }

            if (!ignore)
            {
                float bounds_hit_t = ray_intersect_rect3(o, d, brush->bounds);
                if (bounds_hit_t < t)
                {
                    for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
                    {
                        map_poly_t *poly = &brush->polys[plane->poly_index];

                        uint32_t triangle_count = poly->index_count / 3;
                        for (size_t triangle_index = 0; triangle_index < triangle_count; triangle_index++)
                        {
                            v3_t a = poly->vertices[poly->indices[3*triangle_index + 0]].pos;
                            v3_t b = poly->vertices[poly->indices[3*triangle_index + 1]].pos;
                            v3_t c = poly->vertices[poly->indices[3*triangle_index + 2]].pos;

                            v3_t uvw;
                            float triangle_hit_t = ray_intersect_triangle(o, d, a, b, c, &uvw);

                            if (triangle_hit_t >= min_t && t > triangle_hit_t)
                            {
                                t = triangle_hit_t;
                                hit_brush = brush;
                                hit_plane = plane;
                            }
                        }
                    }
                }
            }
        }
    }

    result->t     = t;
    result->brush = hit_brush;
    result->plane = hit_plane;
    
    return t < max_t;
}

void bake_lighting(const light_bake_params_t *params)
{
    map_t *map = params->map;

    v3_t sun_direction = normalize(params->sun_direction);
    v3_t sun_color     = params->sun_color;

    diag_node_t *light_traces = diag_begin(strlit("light traces"));

    for (map_entity_t *e = map->first_entity; e; e = e->next)
    {
        int brush_index = 0;
        for (map_brush_t *brush = e->first_brush; brush; brush = brush->next, brush_index++)
        {
            diag_node_t *brush_diag = diag_add_group(light_traces, string_format(temp, "brush %d", brush_index));

            v3_t colors[] = {
                { 1, 0, 0 },
                { 0, 1, 0 },
                { 0, 0, 1 },
                { 1, 1, 0 },
                { 1, 0, 1 },
                { 0, 1, 1 },
            };
            v3_t diag_color = colors[brush_index % ARRAY_COUNT(colors)];

            /* diag_add_box(brush_diag, diag_color, rect3_grow_radius(brush->bounds, (v3_t){0.1f, 0.1f, 0.1f})); */

            for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
            {
                plane_t p;
                plane_from_points(plane->a, plane->b, plane->c, &p);

                float n_dot_l = dot(p.n, sun_direction);

                float scale_x = max(1.0f, LIGHTMAP_SCALE * ceilf((plane->tex_maxs.x - plane->tex_mins.x) / LIGHTMAP_SCALE));
                float scale_y = max(1.0f, LIGHTMAP_SCALE * ceilf((plane->tex_maxs.y - plane->tex_mins.y) / LIGHTMAP_SCALE));

                int w = (int)(scale_x / LIGHTMAP_SCALE);
                int h = (int)(scale_y / LIGHTMAP_SCALE);

                uint32_t *lighting = m_alloc_array(temp, w*h, uint32_t);

                int arrow_index = 0;

                for (size_t y = 0; y < h; y++)
                for (size_t x = 0; x < w; x++)
                {
                    float u = ((float)x + 0.5f) / (float)(w);
                    float v = ((float)y + 0.5f) / (float)(h);

                    v3_t world_p = plane->lm_origin;
                    world_p = add(world_p, mul(scale_x*u, plane->s.xyz));
                    world_p = add(world_p, mul(scale_y*v, plane->t.xyz));

                    v3_t intersect_origin = add(world_p, mul(0.1f, p.n));

                    float shadow_accum = 0.0f;
                    int shadow_sample_count = 8;

                    if (n_dot_l > 0.0f)
                    {
                        for (int shadow_samples = 0; shadow_samples < shadow_sample_count; shadow_samples++)
                        {
                            v3_t jitter;
                            do
                            {
                                jitter = (v3_t){
                                    2.0f*((float)rand() / RAND_MAX) - 1.0f,
                                    2.0f*((float)rand() / RAND_MAX) - 1.0f,
                                    2.0f*((float)rand() / RAND_MAX) - 1.0f,
                                };
                            } while(vlensq(jitter) > 1.0f);
                            jitter = mul(0.035f, jitter);

                            v3_t shadow_direction = normalize(add(sun_direction, jitter));

                            intersect_params_t intersect_params = {
                                .o                  = intersect_origin,
                                .d                  = shadow_direction,
                                .ignore_brush_count = 1,
                                .ignore_brushes     = &brush,
                            };

                            intersect_result_t intersect;
                            if (intersect_map(map, &intersect_params, &intersect))
                            {
                                if (false)
                                {
                                    diag_node_t *arrow = diag_add_arrow(brush_diag, diag_color, intersect_origin, add(intersect_origin, mul(intersect.t, shadow_direction)));
                                    diag_set_name(arrow, string_format(temp, "arrow %d", arrow_index++));
                                }
                                shadow_accum += 1.0f;
                            }
                        }
                    }

                    shadow_accum /= (float)shadow_sample_count;

                    float shadow_throughput = 1.0f - shadow_accum;

                    // v3_t up = { 0, 0, 1 };

                    float lambertian = max(0.0f, n_dot_l);
                    float ambient    = 1.0f; // max(0.0f, dot(p.n, up));

                    v4_t color;
                    color.x = sun_color.x*shadow_throughput*lambertian;
                    color.y = sun_color.y*shadow_throughput*lambertian;
                    color.z = sun_color.z*shadow_throughput*lambertian;
                    color.w = 1.0f;

                    color.xyz = add(color.xyz, mul(ambient, params->ambient_color));

                    lighting[y*w + x] = pack_color(color);
                }

                map_poly_t *poly = &brush->polys[plane->poly_index];
                poly->lightmap = render->upload_texture(&(upload_texture_t) {
                    .format = PIXEL_FORMAT_RGBA8,
                    .w      = w,
                    .h      = h,
                    .pitch  = sizeof(uint32_t)*w,
                    .pixels = lighting,
                });
            }
        }
    }
}
