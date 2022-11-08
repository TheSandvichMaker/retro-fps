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
        if (!intersect_map(map, &(intersect_params_t) {
                .o                  = hit_p,
                .d                  = sun_direction,
                .ignore_brush_count = 1,
                .ignore_brushes     = &brush,
                .occlusion_test     = true,
            }, NULL))
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

    // insanity
    arena_t arena;
    image_t result;

} bake_light_job_t;

static void bake_light_job(void *userdata)
{
    bake_light_job_t *job = userdata;

#if DEBUG
    int diffuse_ray_count = 64;
#else
    int diffuse_ray_count = 128;
#endif

    const light_bake_params_t *params = job->params;

    map_t *map = params->map;
    map_brush_t *brush = job->brush;
    map_plane_t *plane = job->plane;

    arena_t *arena = &job->arena;

    random_series_t entropy = {
        (uint32_t)(uintptr_t)plane,
    };

    plane_t p;
    plane_from_points(plane->a, plane->b, plane->c, &p);

    v3_t t, b;
    get_tangent_vectors(p.n, &t, &b);

    v3_t n = p.n;

    float scale_x = max(1.0f, LIGHTMAP_SCALE * ceilf((plane->lm_tex_maxs.x - plane->lm_tex_mins.x) / LIGHTMAP_SCALE));
    float scale_y = max(1.0f, LIGHTMAP_SCALE * ceilf((plane->lm_tex_maxs.y - plane->lm_tex_mins.y) / LIGHTMAP_SCALE));

    int w = (int)(scale_x / LIGHTMAP_SCALE);
    int h = (int)(scale_y / LIGHTMAP_SCALE);

    uint32_t *lighting = m_alloc_array(arena, w*h, uint32_t);

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
        v3_t color_accum = mul(primary_hit_lighting, (float)diffuse_ray_count);

        for (int i = 0; i < diffuse_ray_count; i++)
        {
            v2_t sample = random_unilateral2(&entropy);
            v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

            v3_t dir = mul(unrotated_dir.x, t);
            dir = add(dir, mul(unrotated_dir.y, b));
            dir = add(dir, mul(unrotated_dir.z, n));

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

    job->result = (image_t){
        .w      = w,
        .h      = h,
        .pitch  = sizeof(uint32_t)*w,
        .pixels = lighting,
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

    m_scoped(temp)
    {
        size_t job_count = 0;
        bake_light_job_t *jobs = m_alloc_array(temp, plane_count, bake_light_job_t);

        // silly, silly.
        job_queue_t queue = create_job_queue(6, plane_count);

        for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
        {
            map_brush_t *brush = map->brushes[brush_index];

            for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
            {
                bake_light_job_t *job = &jobs[job_count++];
                job->params = params;
                job->brush  = brush;
                job->plane  = plane;

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
                .format = PIXEL_FORMAT_RGBA8,
                .w      = result->w,
                .h      = result->h,
                .pitch  = sizeof(uint32_t)*result->w,
                .pixels = result->pixels,
            });

            m_release(&job->arena);
        }
    }
}
