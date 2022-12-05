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

typedef struct lum_thread_context_t
{
    arena_t arena;

    random_series_t entropy;

    lum_params_t params;
    lum_debug_data_t debug;
} lum_thread_context_t;

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

static v3_t random_point_on_light(random_series_t *entropy, map_point_light_t *light)
{
    return add(light->p, mul(16.0f, random_in_unit_cube(entropy)));
}

static v3_t evaluate_lighting(lum_thread_context_t *thread, lum_path_vertex_t *path_vertex, v3_t hit_p, v3_t hit_n, bool ignore_sun)
{
    lum_params_t *params = &thread->params;
    map_t *map = params->map;

    map_brush_t *brush = path_vertex->brush;

    v3_t lighting = { 0 };

    v3_t sun_direction = params->sun_direction;
    float sun_ndotl = max(0.0f, dot(sun_direction, hit_n));

    unsigned sample_count = 0;
    lum_light_sample_t *samples = m_alloc_array(&thread->arena, map->light_count + 1, lum_light_sample_t);

    for (size_t i = 0; i < map->light_count; i++)
    {
        map_point_light_t *light = &map->lights[i];

        v3_t light_p = random_point_on_light(&thread->entropy, light);

        v3_t  light_vector   = sub(light_p, hit_p);
        float light_distance = vlen(light_vector);
        v3_t light_direction = div(light_vector, light_distance);
        float light_ndotl    = dot(hit_n, light_direction);

        lum_light_sample_t *sample = &samples[sample_count++];
        sample->d = light_direction;

        if (light_ndotl > 0.0f)
        {
            intersect_result_t shadow_hit;
            if (!intersect_map(map, &(intersect_params_t) {
                    .o                  = hit_p,
                    .d                  = light_direction,
                    .ignore_brush_count = 1,
                    .ignore_brushes     = &brush,
                    .occlusion_test     = true,
                    .max_t              = light_distance,
                }, &shadow_hit))
            {
                v3_t contribution = light->color;

                contribution = mul(contribution, light_ndotl);
                contribution = mul(contribution, 1.0f / (light_distance*light_distance));

                lighting = add(lighting, contribution);

                sample->contribution = contribution;
                sample->shadow_ray_t = FLT_MAX;
            }
            else
            {
                sample->shadow_ray_t = shadow_hit.t;
            }
        }
    }

    if (!ignore_sun && sun_ndotl > 0.0f)
    {
        v3_t sun_d = add(sun_direction, mul(0.1f, random_in_unit_sphere(&thread->entropy)));
        sun_d = normalize(sun_d);

        lum_light_sample_t *sample = &samples[sample_count++];
        sample->d = sun_d;

        intersect_result_t shadow_hit;
        if (!intersect_map(map, &(intersect_params_t) {
                .o                  = hit_p,
                .d                  = sun_d,
                .ignore_brush_count = 1,
                .ignore_brushes     = &brush,
                .occlusion_test     = true,
            }, &shadow_hit))
        {
            v3_t contribution = mul(params->sun_color, sun_ndotl);
            lighting = add(lighting, contribution);

            sample->contribution = contribution;
            sample->shadow_ray_t = FLT_MAX;
        }
        else
        {
            sample->shadow_ray_t = shadow_hit.t;
        }
    }

    path_vertex->light_sample_count = sample_count;
    path_vertex->light_samples      = samples;

    return lighting;
}

static v3_t pathtrace_recursively(lum_thread_context_t *thread, lum_path_t *path, v3_t o, v3_t d, int recursion)
{
    if (recursion >= thread->params.ray_recursion)
        return (v3_t){0,0,0};

    lum_params_t *params = &thread->params;
    map_t *map = params->map;

    arena_t *arena = &thread->arena;
    (void)arena;

    lum_path_vertex_t *prev_vertex = path->last_vertex;
    map_brush_t *brush = prev_vertex->brush;

    random_series_t *entropy = &thread->entropy;

    intersect_params_t intersect_params = {
        .o                  = o,
        .d                  = d,
        .ignore_brush_count = 1,
        .ignore_brushes     = &brush,
    };

    v3_t color = { 0, 0, 0 };

    lum_path_vertex_t *path_vertex = m_alloc_struct(arena, lum_path_vertex_t);
    path->vertex_count++;
    dll_push_back(path->first_vertex, path->last_vertex, path_vertex);

    intersect_result_t hit;
    if (intersect_map(map, &intersect_params, &hit))
    {
        map_poly_t *hit_poly = hit.poly;

        v3_t uvw = hit.uvw;
        uint32_t triangle_offset = hit.triangle_offset;

        uint16_t *indices   = map->indices          + hit_poly->first_index;
        v2_t     *texcoords = map->vertex.texcoords + hit_poly->first_vertex;

        v2_t t0 = texcoords[indices[triangle_offset + 0]];
        v2_t t1 = texcoords[indices[triangle_offset + 1]];
        v2_t t2 = texcoords[indices[triangle_offset + 2]];

        v2_t tex = v2_add3(mul(uvw.x, t0), mul(uvw.y, t1), mul(uvw.z, t2));

        v3_t albedo = {0, 0, 0};

        image_t *texture = &hit_poly->texture_cpu;
        if (texture->pixels)
        {
            // TODO: Enforce pow2 textures?
            uint32_t tex_x = (uint32_t)((float)texture->w*tex.x) % texture->w;
            uint32_t tex_y = (uint32_t)((float)texture->h*tex.y) % texture->h;

            uint32_t *pixels = texture->pixels;

            v4_t texture_sample = unpack_color(pixels[texture->w*tex_y + tex_x]);
            albedo = texture_sample.xyz;
        }

        v3_t n = hit_poly->normal;

        v3_t t, b;
        get_tangent_vectors(n, &t, &b);

        v3_t hit_p = add(intersect_params.o, mul(hit.t, intersect_params.d));

        v3_t lighting = evaluate_lighting(thread, path_vertex, hit_p, n, false);

        v2_t sample = random_unilateral2(entropy);
        v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

        v3_t bounce_dir = mul(unrotated_dir.x, t);
        bounce_dir = add(bounce_dir, mul(unrotated_dir.y, b));
        bounce_dir = add(bounce_dir, mul(unrotated_dir.z, n));

        lighting = add(lighting, mul(albedo, pathtrace_recursively(thread, path, hit_p, bounce_dir, recursion + 1)));
        color = mul(albedo, lighting);

        path_vertex->brush        = hit.brush;
        path_vertex->poly         = hit.poly;
        path_vertex->o            = hit_p;
        path_vertex->throughput   = albedo;
        path_vertex->contribution = lighting;
    }
    else
    {
        // TODO: Sample skybox
        color = params->sky_color;

        path_vertex->o            = add(prev_vertex->o, intersect_params.d);
        path_vertex->contribution = color;
    }

    return color;
}

typedef struct lum_job_t
{
    lum_thread_context_t *thread_contexts;

    uint32_t brush_index;
    uint32_t plane_index;

    // insanity
    image_t result;
} lum_job_t;

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

static void lum_job(job_context_t *job_context, void *userdata)
{
    lum_job_t *job = userdata;

    lum_thread_context_t *thread = &job->thread_contexts[job_context->thread_index];

    lum_params_t *params = &thread->params;
    map_t *map = params->map;

    map_brush_t *brush = &map->brushes[job->brush_index];
    map_plane_t *plane = &map->planes [job->plane_index];
    map_poly_t  *poly  = &map->polys  [job->plane_index];

    arena_t *arena = &thread->arena;

    random_series_t *entropy = &thread->entropy;

    int ray_count     = params->ray_count;

    plane_t p;
    plane_from_points(plane->a, plane->b, plane->c, &p);

    v3_t t, b;
    get_tangent_vectors(p.n, &t, &b);

    v3_t n = p.n;

    float scale_x = plane->lm_scale_x;
    float scale_y = plane->lm_scale_y;

    int w = plane->lm_tex_w;
    int h = plane->lm_tex_h;

    m_scoped(temp)
    {
        v3_t   *direct_lighting_pixels = m_alloc_array(temp, w*h, v3_t);
        v3_t *indirect_lighting_pixels = m_alloc_array(temp, w*h, v3_t);

        /* int arrow_index = 0; */

        for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++)
        {
            float u = ((float)x + 0.5f) / (float)(w);
            float v = ((float)y + 0.5f) / (float)(h);

            v3_t world_p = plane->lm_origin;
            world_p = add(world_p, mul(scale_x*u, plane->lm_s));
            world_p = add(world_p, mul(scale_y*v, plane->lm_t));

            v3_t   direct_lighting = { 0 };
            v3_t indirect_lighting = { 0 };

            for (int i = 0; i < ray_count; i++)
            {
                lum_path_t *path = m_alloc_struct(arena, lum_path_t);
                path->source_pixel = (v2i_t){ (int)x, (int)y };
                sll_push_back(thread->debug.first_path, thread->debug.last_path, path);

                lum_path_vertex_t *path_vertex = m_alloc_struct(arena, lum_path_vertex_t);
                path->vertex_count++;
                dll_push_back(path->first_vertex, path->last_vertex, path_vertex);

                path_vertex->brush        = brush;
                path_vertex->poly         = poly;
                path_vertex->o            = world_p;
                path_vertex->throughput   = make_v3(1, 1, 1);

                v3_t this_direct_lighting = evaluate_lighting(thread, path_vertex, world_p, n, params->use_dynamic_sun_shadows);

                path_vertex->contribution = this_direct_lighting;

                direct_lighting = add(direct_lighting, this_direct_lighting);

                v2_t sample = random_unilateral2(entropy);
                v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

                v3_t dir = mul(unrotated_dir.x, t);
                dir = add(dir, mul(unrotated_dir.y, b));
                dir = add(dir, mul(unrotated_dir.z, n));

                v3_t this_indirect_lighting = pathtrace_recursively(thread, path, world_p, dir, 0);

                path->contribution = add(this_direct_lighting, this_indirect_lighting);

                indirect_lighting = add(indirect_lighting, this_indirect_lighting);
            }

              direct_lighting = mul(  direct_lighting, 1.0f / ray_count);
            indirect_lighting = mul(indirect_lighting, 1.0f / ray_count);

              direct_lighting_pixels[y*w + x] = direct_lighting;
            indirect_lighting_pixels[y*w + x] = indirect_lighting;
        }

#if  1
        for (int k = 0; k < 32 / LIGHTMAP_SCALE; k++)
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
                        color   = add(color, indirect_lighting_pixels[y*w + xo]);
                        weight += 1.0f;
                    }
                }

                color = mul(color, 1.0f / weight);

                indirect_lighting_pixels[y*w + x] = color;
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
                        color   = add(color, indirect_lighting_pixels[yo*w + x]);
                        weight += 1.0f;
                    }
                }

                color = mul(color, 1.0f / weight);

                indirect_lighting_pixels[y*w + x] = color;
            }
        }
#endif

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
                        color   = add(color, direct_lighting_pixels[y*w + xo]);
                        weight += 1.0f;
                    }
                }

                color = mul(color, 1.0f / weight);

                direct_lighting_pixels[y*w + x] = color;
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
                        color   = add(color, direct_lighting_pixels[yo*w + x]);
                        weight += 1.0f;
                    }
                }

                color = mul(color, 1.0f / weight);

                direct_lighting_pixels[y*w + x] = color;
            }
        }

        for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            direct_lighting_pixels[y*w + x] = add(direct_lighting_pixels[y*w + x], indirect_lighting_pixels[y*w + x]);
        }

        uint32_t *packed = m_alloc_array(arena, w*h, uint32_t);

        for (int i = 0; i < w*h; i++)
        {
            packed[i] = pack_r11g11b10f(direct_lighting_pixels[i]);
        }

        job->result = (image_t){
            .w      = w,
            .h      = h,
            .pitch  = sizeof(uint32_t)*w,
            .pixels = packed,
        };
    }

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

typedef struct lum_voxel_cluster_t
{
    struct lum_voxel_cluster_t *next;

    v3i_t position;
    v4_t *voxels;
} lum_voxel_cluster_t;

typedef struct lum_voxel_cluster_list_t
{
    lum_voxel_cluster_t *first;
    lum_voxel_cluster_t *last;
} lum_voxel_cluster_list_t;

static inline lum_voxel_cluster_t *lum_allocate_cluster(const lum_params_t *params, arena_t *arena, lum_voxel_cluster_list_t *list, v3i_t position)
{
    int cluster_size = params->fogmap_cluster_size;

    lum_voxel_cluster_t *cluster = m_alloc_struct_nozero(arena, lum_voxel_cluster_t);
    cluster->next = NULL;
    cluster->position = position;
    cluster->voxels = m_alloc_array_nozero(arena, cluster_size*cluster_size*cluster_size, v4_t); // TODO: Pack as R11G11B10

    sll_push_back(list->first, list->last, cluster);

    return cluster;
}

void trace_volumetric_lighting(const lum_params_t *params, map_t *map)
{
    rect3_t fogmap_bounds = map->bounds;

    map->fogmap_offset = rect3_center(fogmap_bounds);
    map->fogmap_dim    = rect3_dim(fogmap_bounds);

    uint32_t fogmap_resolution_scale = params->fogmap_scale;
    uint32_t width  = (uint32_t)((map->fogmap_dim.x + fogmap_resolution_scale - 1) / fogmap_resolution_scale);
    uint32_t height = (uint32_t)((map->fogmap_dim.y + fogmap_resolution_scale - 1) / fogmap_resolution_scale);
    uint32_t depth  = (uint32_t)((map->fogmap_dim.z + fogmap_resolution_scale - 1) / fogmap_resolution_scale);

    map->fogmap_w = width;
    map->fogmap_h = height;
    map->fogmap_d = depth;

    random_series_t entropy = { 1 };

    m_scoped(temp)
    {
#if 0
        lum_voxel_cluster_list_t clusters = { 0 };

        int cluster_size = params->fogmap_cluster_size;

        size_t cluster_count_x = (width  + cluster_size - 1) / cluster_size;
        size_t cluster_count_y = (height + cluster_size - 1) / cluster_size;
        size_t cluster_count_z = (depth  + cluster_size - 1) / cluster_size;
#endif

        uint32_t *fogmap = m_alloc_array(temp, width*height*depth, uint32_t);

        uint32_t *dst = fogmap;
        for (size_t z = 0; z < depth;  z++)
        for (size_t y = 0; y < height; y++)
        for (size_t x = 0; x < width;  x++)
        {
            v3_t uvw = {
                (float)x / (float)width,
                (float)y / (float)height,
                (float)z / (float)depth,
            };

            v3_t lighting = { 0 };

            int sample_count = params->fog_light_sample_count;
            for (int sample_index = 0; sample_index < sample_count; sample_index++)
            {
                v3_t world_p = v3_add(fogmap_bounds.min, mul(uvw, map->fogmap_dim));

                v3_t variance = mul(0.5f*(float)fogmap_resolution_scale, random_in_unit_cube(&entropy));
                world_p = add(world_p, variance);

                v3_t sample_lighting = { 0 };
                for (size_t light_index = 0; light_index < map->light_count; light_index++)
                {
                    map_point_light_t *light = &map->lights[light_index];

                    v3_t light_p = random_point_on_light(&entropy, light);

                    v3_t  light_vector   = sub(light_p, world_p);
                    float light_distance = vlen(light_vector);
                    v3_t light_direction = div(light_vector, light_distance);

                    intersect_result_t shadow_hit;
                    if (!intersect_map(map, &(intersect_params_t) {
                            .o              = world_p,
                            .d              = light_direction,
                            .occlusion_test = true,
                            .max_t          = light_distance,
                        }, &shadow_hit))
                    {
                        v3_t contribution = light->color;

                        float biased_light_distance = light_distance + 1;
                        contribution = mul(contribution, 1.0f / (biased_light_distance*biased_light_distance));

                        sample_lighting = add(sample_lighting, contribution);
                    }
                }

                intersect_result_t shadow_hit;
                if (!intersect_map(map, &(intersect_params_t) {
                        .o                  = world_p,
                        .d                  = params->sun_direction,
                        .occlusion_test     = true,
                    }, &shadow_hit))
                {
                    v3_t contribution = params->sun_color;
                    sample_lighting = add(sample_lighting, contribution);
                }

                lighting = add(lighting, sample_lighting);
            }
            lighting = mul(lighting, 1.0f / (float)sample_count);

            *dst++ = pack_r11g11b10f(lighting);
        }

        map->fogmap = render->upload_texture(&(upload_texture_t) {
            .desc = {
                .type        = TEXTURE_TYPE_3D,
                .format      = PIXEL_FORMAT_R11G11B10F,
                .w           = width,
                .h           = height,
                .d           = depth,
                .pitch       = sizeof(uint32_t)*width,
                .slice_pitch = sizeof(uint32_t)*width*height,
            },
            .data = {
                .pixels = fogmap,
            },
        });
    }
}

void bake_lighting(const lum_params_t *params_init, lum_results_t *results)
{
    // making a copy of the params so they can be massaged against bad input
    lum_params_t params;
    copy_struct(&params, params_init);

    params.sun_direction = normalize(params.sun_direction);

    map_t *map = params.map;

    m_scoped(temp)
    {
        size_t thread_count = 6;

        size_t job_count = 0;
        lum_job_t *jobs = m_alloc_array(temp, map->plane_count, lum_job_t);

        // silly, silly.
        job_queue_t queue = create_job_queue(thread_count, map->plane_count);

        lum_thread_context_t *thread_contexts = m_alloc_array(temp, thread_count, lum_thread_context_t);

        for (size_t i = 0; i < thread_count; i++)
        {
            lum_thread_context_t *thread_context = &thread_contexts[i];
            copy_struct(&thread_context->params, &params);

            thread_context->entropy.state = (uint32_t)(i + 1);
        }

        for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
        {
            map_brush_t *brush = &map->brushes[brush_index];

            for (size_t plane_index = 0; plane_index < brush->plane_poly_count; plane_index++)
            {
                lum_job_t *job = &jobs[job_count++];
                job->thread_contexts = thread_contexts;
                job->brush_index     = (uint32_t)(brush_index);
                job->plane_index     = (uint32_t)(brush->first_plane_poly + plane_index);

                add_job_to_queue(queue, lum_job, job);
            }
        }

#if 0
        for (size_t job_index = 0; job_index < job_count; job_index++)
        {
            lum_job(&jobs[job_index]);
        }
#else
        wait_on_queue(queue);
#endif

        for (size_t job_index = 0; job_index < job_count; job_index++)
        {
            lum_job_t *job = &jobs[job_index];

            map_poly_t *poly = &map->polys[job->plane_index];

            image_t *result = &job->result;

            poly->lightmap = render->upload_texture(&(upload_texture_t) {
                .desc = {
                    .format = PIXEL_FORMAT_R11G11B10F,
                    .w      = result->w,
                    .h      = result->h,
                    .pitch  = sizeof(uint32_t)*result->w,
                },
                .data = {
                    .pixels = result->pixels,
                },
            });
        }

        if (results)
        {
            lum_debug_data_t *result_debug_data = &results->debug_data;

            for (size_t i = 0; i < thread_count; i++)
            {
                lum_thread_context_t *thread_context = &thread_contexts[i];
                lum_debug_data_t *source_debug_data = &thread_context->debug;

                sll_append(result_debug_data->first_path, result_debug_data->last_path,
                           source_debug_data->first_path, source_debug_data->last_path);

                // m_release(&thread_context->arena);
            }
        }

        destroy_job_queue(queue);
    }

    trace_volumetric_lighting(&params, map);

    map->light_baked = true;
}

// --------------------------------------------------------

#if 0
static v3i_t world_dim_to_lightmap_dim(v3_t dim)
{
    v3i_t result = {
        (int)max(1.0f, ceilf(dim.x / LIGHTMAP_SCALE)),
        (int)max(1.0f, ceilf(dim.y / LIGHTMAP_SCALE)),
        (int)max(1.0f, ceilf(dim.z / LIGHTMAP_SCALE)),
    };
    return result;
}

typedef struct lum_planar_texel_t
{
    struct lum_planar_texel_t *next;
    float depth;
} lum_planar_texel_t;

typedef struct lum_planar_view_t
{
    v2i_t dim;
    lum_planar_texel_t **texels;
} lum_planar_view_t;

static void lum_generate_triplanar_textures(map_t *map)
{
    rect3_t bounds = map->bounds;

    v3i_t dim = world_dim_to_lightmap_dim(rect3_dim(bounds));

    v2i_t plane_dims[] = {
        make_v2(dim.x, dim.y),
        make_v2(dim.z, dim.x),
        make_v2(dim.y, dim.z),
    };

    for (size_t plane_index = 0; plane_index < 3; plane_index++)
    {
        int i1 = (plane_index + 0);
        int i2 = (plane_index + 1) % 3;
        int i3 = (plane_index + 2) % 3;

        v2i_t plane_dim = plane_dims[plane_index];

        for (size_t side = 0; side < 2; side++)
        {
            // basis vectors

            v3_t e1 = {}; 
            e1.e[i1] = 1;

            v3_t e2 = {};
            e2.e[i2] = 1;

            v3_t e3 = {};
            e3.e[i3] = side == 0 ? 1 : -1;

            for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
            {
                map_brush_t *brush = map->brushes[brush_index];

                for (size_t poly_index = 0; poly_index < brush->poly_count; poly_index++)
                {
                    map_poly_t *poly = brush->polys[poly_index];


                }
            }
        }
    }
}
#endif
