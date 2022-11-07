#include <stdlib.h>

#include "light_baker.h"
#include "render.h"
#include "diagram.h"
#include "game/map.h"
#include "game/asset.h"
#include "game/intersect.h"
#include "core/core.h"
#include "core/random.h"

typedef struct intersect_result_t
{
    float t;
    map_poly_t *poly;
    map_brush_t *brush;

    v3_t uvw;
    uint32_t triangle_offset;
} intersect_result_t;

typedef struct intersect_params_t
{
    v3_t o;
    v3_t d;

    bool occlusion_test;

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

    v3_t hit_uvw = {0};
    uint32_t hit_triangle_offset = 0;

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
                                if (params->occlusion_test)
                                    return true;

                                t = triangle_hit_t;
                                hit_brush = brush;
                                hit_poly  = poly;
                                hit_triangle_offset = (uint32_t)(3*triangle_index);
                                hit_uvw = uvw;
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

    result->t               = t;
    result->poly            = hit_poly;
    result->brush           = hit_brush;
    result->triangle_offset = hit_triangle_offset;
    result->uvw             = hit_uvw;
    
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

static v3_t evaluate_lighting(const light_bake_params_t *params, map_t *map, random_series_t *entropy, v3_t hit_p, v3_t hit_n, map_brush_t *brush)
{
    (void)entropy; // will be used later

    v3_t lighting = {0,0,0};

    v3_t sun_direction = normalize(params->sun_direction);
    float ndotl = max(0.0f, dot(sun_direction, hit_n));

    if (ndotl > 0.0f)
    {
        intersect_result_t shadow;
        if (!intersect_map(map, &(intersect_params_t) {
                .o                  = hit_p,
                .d                  = sun_direction,
                .ignore_brush_count = 1,
                .ignore_brushes     = &brush,
                .occlusion_test     = true,
            }, &shadow))
        {
            lighting = params->sun_color;
            lighting = mul(lighting, ndotl);
        }
    }

    return lighting;
}

static v3_t pathtrace_recursively(const light_bake_params_t *params, map_t *map, random_series_t *entropy, v3_t o, v3_t d, map_brush_t *brush, int recursion)
{
    if (recursion == 0) 
        return (v3_t){0,0,0};

    intersect_params_t intersect_params = {
        .o                  = o,
        .d                  = d,
        .ignore_brush_count = 1,
        .ignore_brushes     = &brush,
    };

    v3_t color = { 0, 0, 0 };

    intersect_result_t intersect;
    if (intersect_map(map, &intersect_params, &intersect))
    {
        map_poly_t *hit_poly = intersect.poly;

        v3_t uvw = intersect.uvw;
        uint32_t triangle_offset = intersect.triangle_offset;

        vertex_brush_t t0 = hit_poly->vertices[hit_poly->indices[triangle_offset + 0]];
        vertex_brush_t t1 = hit_poly->vertices[hit_poly->indices[triangle_offset + 0]];
        vertex_brush_t t2 = hit_poly->vertices[hit_poly->indices[triangle_offset + 0]];

        v2_t tex = v2_add3(mul(uvw.x, t0.tex),
                           mul(uvw.y, t1.tex),
                           mul(uvw.z, t2.tex));

        v3_t albedo = {0, 0, 0};

        image_t *texture = &hit_poly->texture_cpu;
        if (texture->pixels)
        {
            // TODO: Enforce pow2 textures?
            uint32_t tex_x = (uint32_t)((float)texture->w*tex.x) % texture->w;
            uint32_t tex_y = (uint32_t)((float)texture->h*tex.y) % texture->h;

            v4_t texture_sample = unpack_color(texture->pixels[texture->w*tex_y + tex_x]);
            albedo = texture_sample.xyz;
        }

        v3_t n = hit_poly->normal;

        v3_t t, b;
        get_tangent_vectors(n, &t, &b);

        v3_t hit_p = add(intersect_params.o, mul(intersect.t, intersect_params.d));

        v3_t lighting = evaluate_lighting(params, map, entropy, hit_p, n, brush);

        v2_t sample = random_unilateral2(entropy);
        v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

        v3_t bounce_dir = mul(unrotated_dir.x, t);
        bounce_dir = add(bounce_dir, mul(unrotated_dir.y, b));
        bounce_dir = add(bounce_dir, mul(unrotated_dir.z, n));

        lighting = add(lighting, mul(albedo, pathtrace_recursively(params, map, entropy, hit_p, bounce_dir, intersect.brush, recursion - 1)));
        color = mul(albedo, lighting);
    }
    else
    {
        // sky
        color = params->ambient_color; // (v3_t){ 1, 1, 1 };
    }

    return color;
}

typedef struct bake_light_job_t
{
    const light_bake_params_t *params;
    map_brush_t *brush;
    map_plane_t *plane;
} bake_light_job_t;

static void bake_light_job(void *userdata)
{
    bake_light_job_t *job = userdata;

#if DEBUG
    int diffuse_ray_count = 16;
#else
    int diffuse_ray_count = 64;
#endif

    const light_bake_params_t *params = job->params;

    map_t *map = params->map;
    map_brush_t *brush = job->brush;
    map_plane_t *plane = job->plane;

    random_series_t entropy = {
        (uint32_t)(uintptr_t)plane,
    };

    m_scoped(temp)
    {
        plane_t p;
        plane_from_points(plane->a, plane->b, plane->c, &p);

        v3_t t, b;
        get_tangent_vectors(p.n, &t, &b);

        v3_t n = p.n;

        float scale_x = max(1.0f, LIGHTMAP_SCALE * ceilf((plane->tex_maxs.x - plane->tex_mins.x) / LIGHTMAP_SCALE));
        float scale_y = max(1.0f, LIGHTMAP_SCALE * ceilf((plane->tex_maxs.y - plane->tex_mins.y) / LIGHTMAP_SCALE));

        int w = (int)(scale_x / LIGHTMAP_SCALE);
        int h = (int)(scale_y / LIGHTMAP_SCALE);

        uint32_t *lighting = m_alloc_array(temp, w*h, uint32_t);

        /* int arrow_index = 0; */

        for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++)
        {
            float u = ((float)x + 0.5f) / (float)(w);
            float v = ((float)y + 0.5f) / (float)(h);

            v3_t world_p = plane->lm_origin;
            world_p = add(world_p, mul(scale_x*u, plane->s.xyz));
            world_p = add(world_p, mul(scale_y*v, plane->t.xyz));

            v3_t primary_hit_lighting = evaluate_lighting(params, map, &entropy, world_p, n, brush);
            v3_t color_accum = {0, 0, 0};

            for (int i = 0; i < diffuse_ray_count; i++)
            {
                v2_t sample = random_unilateral2(&entropy);
                v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

                v3_t dir = mul(unrotated_dir.x, t);
                dir = add(dir, mul(unrotated_dir.y, b));
                dir = add(dir, mul(unrotated_dir.z, n));

                color_accum = add(color_accum, primary_hit_lighting);
                color_accum = add(color_accum, pathtrace_recursively(params, map, &entropy, world_p, dir, brush, 8));
            }

            v4_t color;
            color.xyz = mul(color_accum, 1.0f / diffuse_ray_count);
            color.w   = 1.0f;

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

void bake_lighting(const light_bake_params_t *params)
{
    map_t *map = params->map;

    size_t plane_count = 0;
    for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
    {
        map_brush_t *brush = map->brushes[brush_index];

        for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
        {
            plane_count++;
        }
    }

    size_t job_count = 0;
    bake_light_job_t *jobs = m_alloc_array(temp, plane_count, bake_light_job_t);

    for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
    {
        map_brush_t *brush = map->brushes[brush_index];

        for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
        {
            jobs[job_count++] = (bake_light_job_t) {
                .params = params,
                .brush  = brush,
                .plane  = plane,
            };
            // add_job_to_queue(&queue, bake_light_job, &job);
        }
    }
    // wait_on_queue(&queue);

    for (size_t job_index = 0; job_index < job_count; job_index++)
    {
        bake_light_job(&jobs[job_index]);
    }
}
