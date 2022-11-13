#include <stdlib.h>

#include "light_baker.h"
#include "render.h"
#include "diagram.h"
#include "game/map.h"
#include "game/asset.h"
#include "game/intersect.h"
#include "core/core.h"
#include "core/random.h"
#include "core/job_queue.h"

static inline void lm_record_debug_ray(bake_light_debug_ray_list_t *dest, arena_t *arena, const bake_light_debug_ray_t *init_ray)
{
    bake_light_debug_ray_t *ray = m_alloc_struct(arena, bake_light_debug_ray_t);
    copy_struct(ray, init_ray);
    sll_push_back(dest->first, dest->last, ray);
}

typedef struct bake_light_thread_context_t
{
    arena_t arena;

    bake_light_params_t params;
    bake_light_debug_data_t debug;
} bake_light_thread_context_t;

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

static v3_t evaluate_lighting(bake_light_thread_context_t *context, random_series_t *entropy, v3_t hit_p, v3_t hit_n, map_brush_t *brush, map_poly_t *poly, bool is_primary_hit)
{
    (void)is_primary_hit;
    (void)entropy; // will be used later

    bake_light_params_t *params = &context->params;
    map_t *map = params->map;

    v3_t lighting = {0,0,0};

    v3_t sun_direction = params->sun_direction;
    float sun_ndotl = max(0.0f, dot(sun_direction, hit_n));

    if (sun_ndotl > 0.0f)
    {
        intersect_result_t shadow_intersect;
        if (!intersect_map(map, &(intersect_params_t) {
                .o                  = hit_p,
                .d                  = sun_direction,
                .ignore_brush_count = 1,
                .ignore_brushes     = &brush,
                .occlusion_test     = true,
            }, &shadow_intersect))
        {
            lighting = params->sun_color;
            lighting = mul(lighting, sun_ndotl);

#if 0
            if (is_primary_hit)
            {
                bake_light_debug_data_t *debug = &context->debug;
                lm_record_debug_ray(&debug->direct_rays, &context->arena, &(bake_light_debug_ray_t){
                    .o = hit_p, 
                    .d = sun_direction, 
                    .t = FLT_MAX, 
                    .spawn_brush = brush,
                    .spawn_poly = poly,
                });
            }
#endif
        }
        else
        {
#if 0
            if (is_primary_hit)
            {
                bake_light_debug_data_t *debug = &context->debug;
                lm_record_debug_ray(&debug->direct_rays, &context->arena, &(bake_light_debug_ray_t){
                    .o           = hit_p, 
                    .d           = sun_direction, 
                    .t           = shadow_intersect.t, 
                    .spawn_brush = brush,
                    .spawn_poly  = poly,
                });
            }
#endif
        }
    }

    for (size_t i = 0; i < map->light_count; i++)
    {
        map_point_light_t *light = &map->lights[i];

        v3_t  light_vector   = sub(light->p, hit_p);
        float light_distance = vlen(light_vector);

        v3_t light_direction = div(light_vector, light_distance);

        float light_ndotl    = dot(hit_n, light_direction);

        if (light_ndotl > 0.0f)
        {
            intersect_result_t shadow_intersect;
            if (!intersect_map(map, &(intersect_params_t) {
                    .o                  = hit_p,
                    .d                  = light_direction,
                    .ignore_brush_count = 1,
                    .ignore_brushes     = &brush,
                    .occlusion_test     = true,

                    .max_t              = light_distance,
                }, &shadow_intersect))
            {
                v3_t contribution = light->color;

                contribution = mul(contribution, light_ndotl);
                contribution = mul(contribution, 1.0f / (light_distance*light_distance));

                if (is_primary_hit)
                {
                    bake_light_debug_data_t *debug = &context->debug;
                    lm_record_debug_ray(&debug->direct_rays, &context->arena, &(bake_light_debug_ray_t){
                        .o = hit_p, 
                        .d = light_direction, 
                        .t = FLT_MAX, 
                        .spawn_brush = brush,
                        .spawn_poly = poly,
                    });
                }

                lighting = add(lighting, contribution);
            }
            else
            {
                if (is_primary_hit)
                {
                    bake_light_debug_data_t *debug = &context->debug;
                    lm_record_debug_ray(&debug->direct_rays, &context->arena, &(bake_light_debug_ray_t){
                        .o           = hit_p, 
                        .d           = light_direction, 
                        .t           = shadow_intersect.t, 
                        .spawn_brush = brush,
                        .spawn_poly  = poly,
                    });
                }
            }
        }
    }

    return lighting;
}

static v3_t pathtrace_recursively(bake_light_thread_context_t *context, random_series_t *entropy, v3_t o, v3_t d, map_brush_t *brush, map_poly_t *poly, int recursion)
{
    if (recursion == 0) 
        return (v3_t){0,0,0};

    bake_light_params_t *params = &context->params;
    map_t *map = params->map;

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

        v3_t lighting = evaluate_lighting(context, entropy, hit_p, n, brush, poly, false);

        v2_t sample = random_unilateral2(entropy);
        v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

        v3_t bounce_dir = mul(unrotated_dir.x, t);
        bounce_dir = add(bounce_dir, mul(unrotated_dir.y, b));
        bounce_dir = add(bounce_dir, mul(unrotated_dir.z, n));

        lighting = add(lighting, mul(albedo, pathtrace_recursively(context, entropy, hit_p, bounce_dir, intersect.brush, intersect.poly, recursion - 1)));
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
    bake_light_thread_context_t *thread_contexts;

    map_brush_t *brush;
    map_plane_t *plane;

    // insanity
    image_t result;
} bake_light_job_t;

#if 0
static inline uint32_t pack_lightmap_color(v4_t color)
{
    float max = max(color.x, max(color.y, color.z));

    float maxi = floorf(max);
    float maxf = max - maxi;

    color.x -= maxi;
    color.y -= maxi;
    color.z -= maxi;

    color.x = CLAMP(color.x, 0.0f, 1.0f);
    color.y = CLAMP(color.y, 0.0f, 1.0f);
    color.z = CLAMP(color.z, 0.0f, 1.0f);
    color.w = CLAMP(color.w, 0.0f, 1.0f);

    uint32_t result = (((uint32_t)(255.0f*color.x) <<  0) |
                       ((uint32_t)(255.0f*color.y) <<  8) |
                       ((uint32_t)(255.0f*color.z) << 16) |
                       ((uint32_t)(255.0f*color.w) << 24));
    return result;
}
#endif

static void bake_light_job(job_context_t *job_context, void *userdata)
{
    bake_light_job_t *job = userdata;

    bake_light_thread_context_t *bake_context = &job->thread_contexts[job_context->thread_index];

    bake_light_params_t *params = &bake_context->params;

    map_brush_t *brush = job->brush;
    map_plane_t *plane = job->plane;
    map_poly_t *poly = &brush->polys[plane->poly_index];

    arena_t *arena = &bake_context->arena;

    int ray_count     = params->ray_count;
    int ray_recursion = params->ray_recursion;

    random_series_t entropy = {
        (uint32_t)(uintptr_t)plane,
    };

    plane_t p;
    plane_from_points(plane->a, plane->b, plane->c, &p);

    v3_t t, b;
    get_tangent_vectors(p.n, &t, &b);

    v3_t n = p.n;

    float scale_x = plane->lm_scale_x;
    float scale_y = plane->lm_scale_y;

    int w = plane->lm_tex_w;
    int h = plane->lm_tex_h;

    v4_t   *direct_lighting_pixels = m_alloc_array(arena, w*h, v4_t);
    v4_t *indirect_lighting_pixels = m_alloc_array(arena, w*h, v4_t);

    /* int arrow_index = 0; */

    for (size_t y = 0; y < h; y++)
    for (size_t x = 0; x < w; x++)
    {
        float u = ((float)x + 0.5f) / (float)(w);
        float v = ((float)y + 0.5f) / (float)(h);

        v3_t world_p = plane->lm_origin;
        world_p = add(world_p, mul(scale_x*u, plane->lm_s));
        world_p = add(world_p, mul(scale_y*v, plane->lm_t));

        v3_t   direct_lighting = evaluate_lighting(bake_context, &entropy, world_p, n, brush, poly, true);
        v3_t indirect_lighting    = { 0 }; // mul(primary_hit_lighting, (float)ray_count);

        for (int i = 0; i < ray_count; i++)
        {
            v2_t sample = random_unilateral2(&entropy);
            v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

            v3_t dir = mul(unrotated_dir.x, t);
            dir = add(dir, mul(unrotated_dir.y, b));
            dir = add(dir, mul(unrotated_dir.z, n));

            indirect_lighting = add(indirect_lighting, pathtrace_recursively(bake_context, &entropy, world_p, dir, brush, poly, ray_recursion));
        }

        indirect_lighting = mul(indirect_lighting, 1.0f / ray_count);

          direct_lighting_pixels[y*w + x] = (v4_t){ .xyz = direct_lighting   };
        indirect_lighting_pixels[y*w + x] = (v4_t){ .xyz = indirect_lighting };
    }

    for (int k = 0; k < 32; k++)
    {
        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            v3_t color = { 0 };

            float weight = 0.0f;

            for (int o = -1; o <= 1; o++)
            {
                int xo = x + o;
                if (xo >= 0 && xo < w)
                {
                    color   = add(color, indirect_lighting_pixels[y*w + xo].xyz);
                    weight += 1.0f;
                }
            }

            color = mul(color, 1.0f / weight);

            indirect_lighting_pixels[y*w + x].xyz = color;
        }

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            v3_t color = { 0 };

            float weight = 0.0f;

            for (int o = -1; o <= 1; o++)
            {
                int yo = y + o;
                if (yo >= 0 && yo < h)
                {
                    color   = add(color, indirect_lighting_pixels[yo*w + x].xyz);
                    weight += 1.0f;
                }
            }

            color = mul(color, 1.0f / weight);

            indirect_lighting_pixels[y*w + x].xyz = color;
        }
    }

    for (int k = 0; k < 1; k++)
    {
        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            v3_t color = { 0 };

            float weight = 0.0f;

            for (int o = -1; o <= 1; o++)
            {
                int xo = x + o;
                if (xo >= 0 && xo < w)
                {
                    color   = add(color, direct_lighting_pixels[y*w + xo].xyz);
                    weight += 1.0f;
                }
            }

            color = mul(color, 1.0f / weight);

            direct_lighting_pixels[y*w + x].xyz = color;
        }

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            v3_t color = { 0 };

            float weight = 0.0f;

            for (int o = -1; o <= 1; o++)
            {
                int yo = y + o;
                if (yo >= 0 && yo < h)
                {
                    color   = add(color, direct_lighting_pixels[yo*w + x].xyz);
                    weight += 1.0f;
                }
            }

            color = mul(color, 1.0f / weight);

            direct_lighting_pixels[y*w + x].xyz = color;
        }
    }

    for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
    {
        direct_lighting_pixels[y*w + x].xyz = add(direct_lighting_pixels[y*w + x].xyz, indirect_lighting_pixels[y*w + x].xyz);
        direct_lighting_pixels[y*w + x].w   = 1.0f;
    }

    job->result = (image_t){
        .w      = w,
        .h      = h,
        .pitch  = sizeof(v4_t)*w,
        .pixels = (uint32_t *)direct_lighting_pixels,
    };

#if 0
    // TODO: Make upload_texture thread-safe
    map_poly_t *poly = &brush->polys[plane->poly_index];
    poly->lightmap = render->upload_texture(&(upload_texture_t) {
        .format = PIXEL_FORMAT_RGBA8,
        .w      = w,
        .h      = h,
        .pitch  = sizeof(uint32_t)*w,
        .pixels = lighting,
    });
#endif
}

void bake_lighting(const bake_light_params_t *params_init, bake_light_results_t *results)
{
    // making a copy of the params so they can be massaged against bad input
    bake_light_params_t params;
    copy_struct(&params, params_init);

    params.sun_direction = normalize(params.sun_direction);

    map_t *map = params.map;

    size_t plane_count = 0;
    for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
    {
        map_brush_t *brush = map->brushes[brush_index];

        for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
        {
            plane_count++;
        }
    }

    m_scoped(temp)
    {
        size_t thread_count = 6;

        size_t job_count = 0;
        bake_light_job_t *jobs = m_alloc_array(temp, plane_count, bake_light_job_t);

        // silly, silly.
        job_queue_t queue = create_job_queue(thread_count, plane_count);

        bake_light_thread_context_t *thread_contexts = m_alloc_array(temp, thread_count, bake_light_thread_context_t);

        for (size_t i = 0; i < thread_count; i++)
        {
            bake_light_thread_context_t *thread_context = &thread_contexts[i];
            copy_struct(&thread_context->params, &params);
        }

        for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
        {
            map_brush_t *brush = map->brushes[brush_index];

            for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
            {
                bake_light_job_t *job = &jobs[job_count++];
                job->thread_contexts = thread_contexts;
                job->brush           = brush;
                job->plane           = plane;

                add_job_to_queue(queue, bake_light_job, job);
            }
        }

#if 0
        for (size_t job_index = 0; job_index < job_count; job_index++)
        {
            bake_light_job(&jobs[job_index]);
        }
#else
        wait_on_queue(queue);
#endif

        for (size_t job_index = 0; job_index < job_count; job_index++)
        {
            bake_light_job_t *job = &jobs[job_index];

            map_brush_t *brush = job->brush;
            map_plane_t *plane = job->plane;

            map_poly_t *poly = &brush->polys[plane->poly_index];

            image_t *result = &job->result;

            poly->lightmap = render->upload_texture(&(upload_texture_t) {
                .desc = {
                    .format = PIXEL_FORMAT_R32G32B32A32,
                    .w      = result->w,
                    .h      = result->h,
                    .pitch  = sizeof(v4_t)*result->w,
                },
                .data = {
                    .pixels = result->pixels,
                },
            });

        }

        if (results)
        {
            bake_light_debug_data_t *result_debug_data = &results->debug_data;

            for (size_t i = 0; i < thread_count; i++)
            {
                bake_light_thread_context_t *thread_context = &thread_contexts[i];
                bake_light_debug_data_t *source_debug_data = &thread_context->debug;

                sll_append(result_debug_data->direct_rays.first, result_debug_data->direct_rays.last,
                           source_debug_data->direct_rays.first, source_debug_data->direct_rays.last);

                sll_append(result_debug_data->indirect_rays.first, result_debug_data->indirect_rays.last,
                           source_debug_data->indirect_rays.first, source_debug_data->indirect_rays.last);

                // m_release(&thread_context->arena);
            }
        }

        destroy_job_queue(queue);
    }
}
