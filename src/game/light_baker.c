// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

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

static v3_t evaluate_lighting(lum_thread_context_t *thread, lum_params_t *params, lum_path_vertex_t *path_vertex, v3_t hit_p, v3_t hit_n, bool ignore_sun)
{
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
        float light_distance = flt_max(0.0001f, vlen(light_vector));
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
                contribution = mul(contribution, 1.0f / (1.0f + light_distance*light_distance));

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
        v3_t sun_d = sun_direction;
        // sun_d = add(sun_d, mul(0.1f, random_in_unit_sphere(&thread->entropy)));
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

static v3_t pathtrace_recursively(lum_thread_context_t *thread, lum_params_t *params, lum_path_t *path, v3_t o, v3_t d, int recursion)
{
    if (recursion >= params->ray_recursion)
        return (v3_t){0,0,0};

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

	bool ignore_sun = false;

	if (vlen(params->sun_color) <= 0.0001)
	{
		ignore_sun = true;
	}

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

        asset_image_t *texture = get_image(hit_poly->texture);
        if (texture->mips[0].pixels)
        {
            // TODO: Enforce pow2 textures?
            uint32_t tex_x = (uint32_t)((float)texture->w*tex.x) % texture->w;
            uint32_t tex_y = (uint32_t)((float)texture->h*tex.y) % texture->h;

            uint32_t *pixels = texture->mips[0].pixels;

            v4_t texture_sample = unpack_color(pixels[texture->w*tex_y + tex_x]);
            albedo = texture_sample.xyz;
        }

        v3_t n = hit_poly->normal;

        v3_t t, b;
        get_tangent_vectors(n, &t, &b);

        v3_t hit_p = add(intersect_params.o, mul(hit.t, intersect_params.d));

        v3_t lighting = evaluate_lighting(thread, params, path_vertex, hit_p, n, ignore_sun);

        v2_t sample = random_unilateral2(entropy);
        v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

        v3_t bounce_dir = mul(unrotated_dir.x, t);
        bounce_dir = add(bounce_dir, mul(unrotated_dir.y, b));
        bounce_dir = add(bounce_dir, mul(unrotated_dir.z, n));

        lighting = add(lighting, mul(albedo, pathtrace_recursively(thread, params, path, hit_p, bounce_dir, recursion + 1)));
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

	if (v3_contains_nan(color))
	{
		DEBUG_BREAK_ONCE();
	}

    return color;
}

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
	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

    lum_job_t *job = userdata;

	lum_bake_state_t     *state  = job->state;
	lum_params_t         *params = &state->params;
    lum_thread_context_t *thread = &job->thread_contexts[job_context->thread_index];

    if (atomic_load(&state->flags) & LumStateFlag_cancel)
        goto done;

    map_t *map = params->map;
    map_brush_t *brush = &map->brushes[job->brush_index];
    map_plane_t *plane = &map->planes [job->plane_index];
    map_poly_t  *poly  = &map->polys  [job->plane_index];

    arena_t *thread_arena = &thread->arena;

    random_series_t *entropy = &thread->entropy;

    int ray_count = params->ray_count;

    plane_t p;
    plane_from_points(plane->a, plane->b, plane->c, &p);

    v3_t t, b;
    get_tangent_vectors(p.n, &t, &b);

    v3_t n = p.n;

    float scale_x = plane->lm_scale_x;
    float scale_y = plane->lm_scale_y;

    int w = plane->lm_tex_w;
    int h = plane->lm_tex_h;

    v3_t   *direct_lighting_pixels = m_alloc_array(temp, w*h, v3_t);
    v3_t *indirect_lighting_pixels = m_alloc_array(temp, w*h, v3_t);

    /* int arrow_index = 0; */

    for (size_t y = 0; y < h; y++)
    for (size_t x = 0; x < w; x++)
    {
		if (atomic_load(&state->flags) & LumStateFlag_cancel)
			goto done;

        float u = ((float)x + 0.5f) / (float)(w);
        float v = ((float)y + 0.5f) / (float)(h);

        v3_t world_p = plane->lm_origin;
        world_p = add(world_p, mul(scale_x*u, plane->lm_s));
        world_p = add(world_p, mul(scale_y*v, plane->lm_t));

        v3_t   direct_lighting = { 0 };
        v3_t indirect_lighting = { 0 };

        for (int i = 0; i < ray_count; i++)
        {
            lum_path_t *path = m_alloc_struct(thread_arena, lum_path_t);
            path->source_pixel = (v2i_t){ (int)x, (int)y };
            sll_push_back(thread->debug.first_path, thread->debug.last_path, path);

            lum_path_vertex_t *path_vertex = m_alloc_struct(thread_arena, lum_path_vertex_t);
            path->vertex_count++;
            dll_push_back(path->first_vertex, path->last_vertex, path_vertex);

            path_vertex->brush        = brush;
            path_vertex->poly         = poly;
            path_vertex->o            = world_p;
            path_vertex->throughput   = make_v3(1, 1, 1);

            v3_t this_direct_lighting = evaluate_lighting(thread, params, path_vertex, world_p, n, params->use_dynamic_sun_shadows);

            path_vertex->contribution = this_direct_lighting;

            direct_lighting = add(direct_lighting, this_direct_lighting);

            v2_t sample = random_unilateral2(entropy);
            v3_t unrotated_dir = map_to_cosine_weighted_hemisphere(sample);

            v3_t dir = mul(unrotated_dir.x, t);
            dir = add(dir, mul(unrotated_dir.y, b));
            dir = add(dir, mul(unrotated_dir.z, n));

            v3_t this_indirect_lighting = pathtrace_recursively(thread, params, path, world_p, dir, 0);

            path->contribution = add(this_direct_lighting, this_indirect_lighting);

            indirect_lighting = add(indirect_lighting, this_indirect_lighting);
        }

		DEBUG_ASSERT(!v3_contains_nan(  direct_lighting));
		DEBUG_ASSERT(!v3_contains_nan(indirect_lighting));

          direct_lighting = mul(  direct_lighting, 1.0f / ray_count);
        indirect_lighting = mul(indirect_lighting, 1.0f / ray_count);

          direct_lighting_pixels[y*w + x] = direct_lighting;
        indirect_lighting_pixels[y*w + x] = indirect_lighting;
    }

	if (atomic_load(&state->flags) & LumStateFlag_cancel)
        goto done;

#if  0
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

	if (atomic_load(&state->flags) & LumStateFlag_cancel)
        goto done;

#if 0
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
#endif

	if (atomic_load(&state->flags) & LumStateFlag_cancel)
        goto done;

    for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
    {
        direct_lighting_pixels[y*w + x] = add(direct_lighting_pixels[y*w + x], indirect_lighting_pixels[y*w + x]);
    }

	if (atomic_load(&state->flags) & LumStateFlag_cancel)
        goto done;

#if 1
    uint32_t *packed = m_alloc_array(temp, w*h, uint32_t);

    for (int i = 0; i < w*h; i++)
    {
        packed[i] = pack_r11g11b10f(direct_lighting_pixels[i]);
    }
#else
    v4_t *packed = m_alloc_array(temp, w*h, v4_t);

    for (int i = 0; i < w*h; i++)
    {
		v3_t c = direct_lighting_pixels[i];
        packed[i] = make_v4(c.x, c.y, c.z, 1.0f);
    }
#endif

    if (RESOURCE_HANDLE_VALID(poly->lightmap_rhi))
    {
		rhi_destroy_texture(poly->lightmap_rhi);
    }

	rhi_texture_t lightmap_rhi = rhi_create_texture(&(rhi_create_texture_params_t){
		.debug_name = S("lightmap_texture"),
		.dimension	= RhiTextureDimension_2d,
		.width      = w,
		.height     = h,
		.format     = PixelFormat_r11g11b10_float,
		.initial_data = &(rhi_texture_data_t){
			.subresources      = &packed,
			.subresource_count = 1,
			.row_stride        = sizeof(packed[0])*w,
		},
	});

	rhi_wait_on_texture_upload(lightmap_rhi);

	poly->lightmap_rhi = lightmap_rhi;

done:
	atomic_fetch_add(&state->jobs_completed, 1);

	if (bake_jobs_completed(state))
	{
		bake_finalize(state);
	}

	m_scope_end(temp);
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
    cluster->next     = NULL;
    cluster->position = position;
    cluster->voxels   = m_alloc_array_nozero(arena, cluster_size*cluster_size*cluster_size, v4_t); // TODO: Pack as R11G11B10

    sll_push_back(list->first, list->last, cluster);

    return cluster;
}

static void trace_volumetric_lighting_job(job_context_t *job_context, void *userdata)
{
	(void)job_context;

	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

	lum_bake_state_t *state  = userdata;
	lum_params_t     *params = &state->params;
	map_t            *map    = params->map;

	if (atomic_load(&state->flags) & LumStateFlag_cancel)
        goto done;

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

    v4_t *fogmap = m_alloc_array(temp, width*height*depth, v4_t);

    v4_t *dst = fogmap;
    for (size_t z = 0; z < depth;  z++)
    for (size_t y = 0; y < height; y++)
    for (size_t x = 0; x < width;  x++)
    {
		if (atomic_load(&state->flags) & LumStateFlag_cancel)
            goto done;

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

            if (!params->use_dynamic_sun_shadows)
            {
                intersect_result_t shadow_hit;
                if (!intersect_map(map, &(intersect_params_t) {
                        .o              = world_p,
                        .d              = params->sun_direction,
                        .occlusion_test = true,
                    }, &shadow_hit))
                {
                    v3_t contribution = params->sun_color;
                    sample_lighting = add(sample_lighting, contribution);
                }
            }

            lighting = add(lighting, sample_lighting);
        }
        lighting = mul(lighting, 1.0f / (float)sample_count);

        *dst++ = (v4_t){.xyz=lighting, .w0=1.0}; // pack_r11g11b10f(lighting);
    }

#if 0
    if (RESOURCE_HANDLE_VALID(map->fogmap))
    {
        render->destroy_texture(map->fogmap);
    }
#endif

#if 0
    map->fogmap = render->upload_texture(&(r_upload_texture_t) {
        .desc = {
            .type        = R_TEXTURE_TYPE_3D,
            .format      = R_PIXEL_FORMAT_R32G32B32A32F,
            .w           = width,
            .h           = height,
            .d           = depth,
        },
        .data = {
            .pitch       = sizeof(fogmap[0])*width,
            .slice_pitch = sizeof(fogmap[0])*width*height,
            .pixels      = fogmap,
        },
    });
#endif

done:
	atomic_fetch_add(&state->jobs_completed, 1);

	if (bake_jobs_completed(state))
	{
		bake_finalize(state);
	}

	m_scope_end(temp);
}

lum_bake_state_t *bake_lighting(const lum_params_t *in_params)
{
	lum_bake_state_t *state = m_bootstrap(lum_bake_state_t, arena);
	copy_struct(&state->params, in_params);

	state->start_time = os_hires_time();

	arena_t      *arena  = &state->arena;
    lum_params_t *params = &state->params;

    params->sun_direction = normalize(params->sun_direction);

    map_t *map = params->map;

	job_queue_t queue = high_priority_job_queue;

	lum_job_t *jobs = m_alloc_array(arena, map->plane_count, lum_job_t);

	state->thread_count = (uint32_t)get_job_queue_thread_count(queue);
	state->thread_contexts = m_alloc_array(arena, state->thread_count, lum_thread_context_t);

	for (size_t i = 0; i < state->thread_count; i++)
	{
		lum_thread_context_t *thread_context = &state->thread_contexts[i];
		thread_context->entropy.state = (uint32_t)(i + 1);
	}

	state->job_count++;
	add_job_to_queue(queue, trace_volumetric_lighting_job, state);

	for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
	{
		map_brush_t *brush = &map->brushes[brush_index];

		for (size_t plane_index = 0; plane_index < brush->plane_poly_count; plane_index++)
		{
			lum_job_t *job = &jobs[state->job_count++ - 1]; // goofy, minus 1 because I added the volumetric job first (because it's slower, so better to start early)
			job->thread_contexts = state->thread_contexts;
			job->state           = state;
			job->brush_index     = (uint32_t)(brush_index);
			job->plane_index     = (uint32_t)(brush->first_plane_poly + plane_index);

			add_job_to_queue(queue, lum_job, job);
		}
	}

	return state;
}

bool bake_finalize(lum_bake_state_t *state)
{
    bool result = false;

	lum_state_flags_t flags = atomic_load(&state->flags);

    if (flags & LumStateFlag_cancel)
    {
        release_bake_state(state);
    } 
    else if (!(flags & LumStateFlag_finalized) && bake_jobs_completed(state))
	{
		lum_debug_data_t *debug = &state->results.debug;

		for (size_t i = 0; i < state->thread_count; i++)
		{
			lum_thread_context_t *thread_context = &state->thread_contexts[i];
			lum_debug_data_t *thread_debug = &thread_context->debug;

			sll_append(       debug->first_path,        debug->last_path,
					   thread_debug->first_path, thread_debug->last_path);

			// TODO: Copy debug data to state arena
			// m_release(&thread_context->arena);
		}

		state->end_time = os_hires_time();
		state->final_bake_time = os_seconds_elapsed(state->start_time, state->end_time);

		atomic_fetch_or(&state->flags, LumStateFlag_finalized);

        result = true;
	}

	return result;
}

void bake_cancel(lum_bake_state_t *state)
{
	atomic_fetch_or(&state->flags, LumStateFlag_cancel);
}

bool release_bake_state(lum_bake_state_t *state)
{
	bool result = false;

	lum_state_flags_t flags = atomic_load(&state->flags);

	if ((flags & LumStateFlag_finalized) || ((flags & LumStateFlag_cancel) && bake_jobs_completed(state)))
	{
		result = true;
		m_release(&state->arena);
	}

	return result;
}
