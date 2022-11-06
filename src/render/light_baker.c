#include <stdlib.h>

#include "light_baker.h"
#include "render.h"
#include "diagram.h"
#include "game/map.h"
#include "game/asset.h"
#include "game/intersect.h"
#include "core/core.h"
#include "core/random.h"

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
    map_poly_t *poly;
    map_brush_t *brush;
} intersect_result_t;

typedef struct intersect_params_t
{
    v3_t o;
    v3_t d;

    float min_t, max_t;

    size_t ignore_brush_count;
    map_brush_t **ignore_brushes;
} intersect_params_t;

#if 0
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
#endif

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
    map_poly_t  *hit_poly  = NULL;

    bool d_is_negative[3] = {
        d.x < 0.0f,
        d.y < 0.0f,
        d.z < 0.0f,
    };

    uint32_t node_stack_at = 0;
    uint32_t node_stack[64];

    node_stack[node_stack_at++] = 0;

    while (node_stack_at > 0)
    {
        uint32_t node_index = node_stack[--node_stack_at];

        map_bvh_node_t *node = &map->nodes[node_index];

        float bounds_hit_t = ray_intersect_rect3(o, d, node->bounds);
        if (bounds_hit_t < t)
        {
            if (node->count > 0)
            {
                uint32_t first = node->left_first;
                uint16_t count = node->count;

                for (size_t brush_index = 0; brush_index < count; brush_index++)
                {
                    map_brush_t *brush = map->brushes[first + brush_index];

                    for (size_t poly_index = 0; poly_index < brush->poly_count; poly_index++)
                    {
                        map_poly_t *poly = &brush->polys[poly_index];

                        uint32_t triangle_count = poly->index_count / 3;
                        for (size_t triangle_index = 0; triangle_index < triangle_count; triangle_index++)
                        {
                            v3_t a = poly->vertices[poly->indices[3*triangle_index + 0]].pos;
                            v3_t b = poly->vertices[poly->indices[3*triangle_index + 1]].pos;
                            v3_t c = poly->vertices[poly->indices[3*triangle_index + 2]].pos;

                            v3_t uvw;
                            float triangle_hit_t = ray_intersect_triangle(o, d, a, b, c, &uvw);

                            if (triangle_hit_t >= min_t &&
                                triangle_hit_t < t)
                            {
                                t = triangle_hit_t;
                                hit_brush = brush;
                                hit_poly  = poly;
                            }
                        }
                    }
                }
            }
            else
            {
                uint32_t left = node->left_first;

                if (d_is_negative[node->split_axis])
                {
                    node_stack[node_stack_at++] = left;
                    node_stack[node_stack_at++] = left + 1;
                }
                else
                {
                    node_stack[node_stack_at++] = left + 1;
                    node_stack[node_stack_at++] = left;
                }
            }
        }
    }

    result->t    = t;
    result->poly = hit_poly;
    result->brush = hit_brush;
    
    return t < max_t;
}

v3_t map_to_cosine_weighted_hemisphere(v2_t sample)
{
    float azimuth = 2.0f*PI32*sample.x;
    float y       = sample.y;
    float a       = sqrt_ss(1.0f - y);

    float s, c;
    sincos_ss(azimuth, &s, &c);

    v3_t result = {
        .x = s*a,
        .y = c*a,
        .z = sqrt_ss(y),
    };

    return result;
}

void bake_lighting(const light_bake_params_t *params)
{
    random_series_t entropy = { 1 };

    map_t *map = params->map;

    v3_t sun_direction = normalize(params->sun_direction);
    v3_t sun_color     = params->sun_color;

    diag_node_t *light_traces = diag_begin(strlit("light traces"));

#if DEBUG
    int shadow_sample_count = 8;
#else
    int shadow_sample_count = 32;
#endif
    v3_t *jitters = m_alloc_array(temp, shadow_sample_count, v3_t);

#if DEBUG
    int diffuse_ray_count = 16;
#else
    int diffuse_ray_count = 64;
#endif

    for (int i = 0; i < shadow_sample_count; i++)
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

        jitters[i] = jitter;
    }

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
                m_scoped(temp)
                {
                    plane_t p;
                    plane_from_points(plane->a, plane->b, plane->c, &p);

                    v3_t t, b;
                    get_tangent_vectors(p.n, &t, &b);

                    v3_t n = p.n;

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

                        if ((x == 0 || x == w - 1) &&
                            (y == 0 || y == h - 1))
                        {
                            // diag_add_text(brush_diag, diag_color, world_p, 
                              //            string_format(temp, "%f, %f, %f", world_p.x, world_p.y, world_p.z));
                        }

                        v3_t intersect_origin = add(world_p, mul(0.1f, p.n));

                        float shadow_accum = 0.0f;

                        if (n_dot_l > 0.0f)
                        {
                            for (int shadow_sample_index = 0; shadow_sample_index < shadow_sample_count; shadow_sample_index++)
                            {
                                v3_t jitter = jitters[shadow_sample_index];
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

                        float ao = 0.0f;

                        for (int i = 0; i < diffuse_ray_count; i++)
                        {
                            v2_t sample = random_unilateral2(&entropy);
                            v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

                            v3_t dir = mul(unrotated_dir.x, t);
                            dir = add(dir, mul(unrotated_dir.y, b));
                            dir = add(dir, mul(unrotated_dir.z, n));

                            // diag_add_arrow(brush_diag, diag_color, intersect_origin, add(intersect_origin, mul(10.0f, dir)));

                            intersect_params_t intersect_params = {
                                .o                  = intersect_origin,
                                .d                  = dir,
                                .ignore_brush_count = 1,
                                .ignore_brushes     = &brush,
                            };

                            intersect_result_t intersect;
                            if (intersect_map(map, &intersect_params, &intersect))
                            {
                                float max_ao_distance = 75.0f;
                                float ao_t = min(intersect.t / max_ao_distance, 1.0f);

                                if (intersect.t <= max_ao_distance)
                                {
                                    // diag_add_arrow(brush_diag, diag_color, intersect_origin, add(intersect_origin, mul(intersect.t, dir)));
                                }
                                
                                ao += 1.0f - ao_t;
                            }
                            else
                            {
                                ao += 1.0f;
                            }
                        }

                        ao /= (float)diffuse_ray_count;

                        shadow_accum /= (float)shadow_sample_count;

                        float shadow_throughput = 1.0f - shadow_accum;

                        // v3_t up = { 0, 0, 1 };

                        float lambertian = max(0.0f, n_dot_l);
                        float ambient    = 1.0f - ao; // max(0.0f, dot(p.n, up));

                        v4_t color;
                        color.x = sun_color.x*shadow_throughput*lambertian;
                        color.y = sun_color.y*shadow_throughput*lambertian;
                        color.z = sun_color.z*shadow_throughput*lambertian;
                        color.w = 1.0f;

                        color.xyz = add(color.xyz, mul(ambient, params->ambient_color));

                        v3_t dither = random_in_unit_cube(&entropy);

                        color.x += (dither.x - 0.5f) / 255.0f;
                        color.y += (dither.y - 0.5f) / 255.0f;
                        color.z += (dither.z - 0.5f) / 255.0f;

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
}
