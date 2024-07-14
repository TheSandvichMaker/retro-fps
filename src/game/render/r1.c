#include "r1.h"

#include "shaders/gen/shaders.c"

//------------------------------------------------------------------------
// CVars

CVAR_BOOL  (cvar_r1_ui_heat_map,            "r1.ui.heat_map",                   false);
CVAR_I32_EX(cvar_r1_ui_heat_map_scale,      "r1.ui.heat_map_scale",             16, 1, 255);
CVAR_I32_EX(cvar_r1_bloom_downsample_steps, "r1.bloom.filter_downsample_steps", 8, 1, 8);
CVAR_I32_EX(cvar_r1_bloom_upsample_steps,   "r1.bloom.filter_upsample_steps",   4, 1, 8);
CVAR_F32_EX(cvar_r1_bloom_amount,           "r1.bloom.amount",                  0.05f, 0.0f, 1.0f);
CVAR_BOOL  (cvar_r1_bloom_preview,          "r1.bloom.preview",                 false);

//------------------------------------------------------------------------

fn_local void r1_load_all_shaders(void);
fn_local void r1_load_all_psos(uint32_t multisample_count);

void r1_equip(r1_state_t *state)
{
	if (r1) log(Renderer, Warning, "r1_equip called when a renderer was already equipped for this thread");
	r1 = state;
}

void r1_unequip(void)
{
	if (!r1) log(Renderer, Warning, "r1_unequip called when a renderer was not equipped for this thread");
	r1 = NULL;
}

r1_state_t *r1_make(void)
{
	//------------------------------------------------------------------------
	// Register CVars
	//------------------------------------------------------------------------

	cvar_register(&cvar_r1_ui_heat_map);
	cvar_register(&cvar_r1_ui_heat_map_scale);
	cvar_register(&cvar_r1_bloom_downsample_steps);
	cvar_register(&cvar_r1_bloom_upsample_steps);
	cvar_register(&cvar_r1_bloom_amount);
	cvar_register(&cvar_r1_bloom_preview);

	//------------------------------------------------------------------------
	// Bootstrap
	//------------------------------------------------------------------------

	r1_state_t *state = m_bootstrap(r1_state_t, arena);
	r1_equip(state);

	r1->debug_drawing_enabled = true;

	r1->push_constants_arena = m_child_arena(&r1->arena, MB(2));

	//------------------------------------------------------------------------
	// Initialize Timestamps
	//------------------------------------------------------------------------

	const uint32_t max_timestamps = 2*R1MaxRegions; // one for region start, one for region end

	rhi_init_timestamps(max_timestamps);
	r1->timestamp_frequency = rhi_get_timestamp_frequency();

	//------------------------------------------------------------------------
	// Initialize Constants
	//------------------------------------------------------------------------

	r1->multisample_count = 8;
	r1->unbounded_scissor_rect = (rect2i_t){ .min = { 0, 0 }, .max = { INT32_MAX, INT32_MAX } };
	
	//------------------------------------------------------------------------
	// Load Shaders
	//------------------------------------------------------------------------

	r1_load_all_shaders();
	r1_load_all_psos(r1->multisample_count);

	//------------------------------------------------------------------------
	// Built-in Textures
	//------------------------------------------------------------------------

	// White

	uint32_t white_pixel = 0xFFFFFFFF;

	r1->white_texture = rhi_create_texture(&(rhi_create_texture_params_t){
	    .debug_name = S("tex_builtin_white"),
		.dimension = RhiTextureDimension_2d,
		.width     = 1,
		.height    = 1,
		.format    = PixelFormat_r8g8b8a8_unorm,
		.initial_data = &(rhi_texture_data_t){
			.subresources      = (uint32_t *[]) { &white_pixel },
			.subresource_count = 1,
			.row_stride        = sizeof(white_pixel),
		},
	});

	r1->white_texture_srv = rhi_get_texture_srv(r1->white_texture);

	// Black

	uint32_t black_pixel = 0x0;

	r1->black_texture = rhi_create_texture(&(rhi_create_texture_params_t){
	    .debug_name = S("tex_builtin_black"),
		.dimension = RhiTextureDimension_2d,
		.width     = 1,
		.height    = 1,
		.format    = PixelFormat_r8g8b8a8_unorm,
		.initial_data = &(rhi_texture_data_t){
			.subresources      = (uint32_t *[]) { &black_pixel },
			.subresource_count = 1,
			.row_stride        = sizeof(black_pixel),
		},
	});

	r1->black_texture_srv = rhi_get_texture_srv(r1->black_texture);

	// Missing

	uint32_t missing_texture_pixels[] = {
		0xFFFF00FF, 0xFF000000,
		0xFF000000, 0xFFFF00FF,
	};

	r1->missing_texture = rhi_create_texture(&(rhi_create_texture_params_t){
	    .debug_name = S("tex_buitin_missing"),
		.dimension = RhiTextureDimension_2d,
		.width     = 2,
		.height    = 2,
		.format    = PixelFormat_r8g8b8a8_unorm,
		.initial_data = &(rhi_texture_data_t){
			.subresources      = (uint32_t *[]) { missing_texture_pixels },
			.subresource_count = 1,
			.row_stride        = sizeof(uint32_t)*2,
		},
	});

	r1->missing_texture_srv = rhi_get_texture_srv(r1->missing_texture);

	// Blue Noise

	for (size_t i = 0; i < ARRAY_COUNT(r1->blue_noise); i++)
	{
		asset_hash_t   hash  = asset_hash_from_string(Sf("gamedata/textures/noise/LDR_RGBA_%zu.png", i));
		asset_image_t *image = get_image_blocking(hash);

		r1->blue_noise    [i] = image->rhi_texture;
		r1->blue_noise_srv[i] = rhi_get_texture_srv(r1->blue_noise[i]);
	}

	//------------------------------------------------------------------------
	// Make render targets and buffers
	//------------------------------------------------------------------------

	r1->shadow_map_resolution = 2048;
	r1->shadow_map = rhi_create_texture(&(rhi_create_texture_params_t){
	    .debug_name = S("sun_shadow_map"),
		.dimension  = RhiTextureDimension_2d,
		.width      = r1->shadow_map_resolution,
		.height     = r1->shadow_map_resolution,
		.usage      = RhiTextureUsage_depth_stencil,
		.format     = PixelFormat_d24_unorm_s8_uint,
	});

	r1->debug_lines = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("debug_lines"),
		.desc = {
			.first_element  = 0,
			.element_count  = (uint32_t)ARRAY_COUNT(debug_lines),
			.element_stride = sizeof(debug_line_t),
		},
	});

	/*
	r1->indirect_args_capacity = 8192;
	r1->indirect_args_count    = 0;

	r1->args = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("indirect_args"),
		.desc = {
			.first_element = 0,
			.element_count = r1->indirect_args_capacity,
			.element_stride = sizeof(rhi_indirect_draw_t),
		},
		.heap  = RhiHeapKind_upload,
		.flags = RhiResourceFlag_dynamic,
	});
	*/

	r1_unequip();

	return state;
}

//------------------------------------------------------------------------

void r1_load_all_shaders(void)
{
	for (size_t i = 1; i < DfShader_COUNT; i++)
	{
		df_shader_info_t *info = &df_shaders[i];

		if (ALWAYS(!info->loaded))
		{
			string_t source = info->hlsl_source;
			info->bytecode = rhi_compile_shader(&r1->arena, source, info->name, info->entry_point, info->target);

			if (info->bytecode.bytes != NULL)
			{
				info->loaded = true;
			}
		}
	}
}

void r1_load_all_psos(uint32_t multisample_count)
{
	for (size_t i = 1; i < DfPso_COUNT; i++)
	{
		df_pso_info_t *info = &df_psos[i];

		if (RESOURCE_HANDLE_VALID(r1->psos[i]))
		{
			rhi_destroy_pso(r1->psos[i]);
		}

		rhi_create_graphics_pso_params_t params = info->params_without_bytecode;
		params.vs = df_shaders[info->vs_index].bytecode;
		params.ps = df_shaders[info->ps_index].bytecode;

		if (params.rasterizer.multisample_enable)
		{
			params.multisample_count = multisample_count;
		}

		r1->psos[i] = rhi_create_graphics_pso(&params);
	}
}

//------------------------------------------------------------------------

void r1_set_pso(rhi_command_list_t *list, df_pso_ident_t pso)
{
	rhi_set_pso(list, r1->psos[pso]);
}

//------------------------------------------------------------------------
// TODO: The timed region stuff is pretty sketch. Need to look at it again

fn_local uint32_t r1_begin_timed_region(rhi_command_list_t *list, string_t identifier)
{
	const uint32_t region_index       = r1->next_region_index++;
	const uint32_t start_timing_index = 2*region_index + 0;

	string_into_storage(r1->region_identifiers[region_index], identifier);

	rhi_record_timestamp(list, start_timing_index);
	rhi_begin_region    (list, identifier);

	return region_index;
}

fn_local void r1_end_timed_region(rhi_command_list_t *list, uint32_t region_index)
{
	const uint32_t end_timing_index = 2*region_index + 1;

	rhi_record_timestamp(list, end_timing_index);
	rhi_end_region      (list);
}

#define R1_TIMED_REGION(list, identifier)                                   \
	for (uint32_t region_index__ = r1_begin_timed_region(list, identifier); \
		 region_index__ != 0xFFFFFFFF;                                      \
		 r1_end_timed_region(list, region_index__), region_index__ = 0xFFFFFFFF)

r1_stats_t r1_report_stats(arena_t *arena)
{
	// TODO: This actually reports stats that are one or two frames behind, but the code makes no effort to
	// buffer that up properly. So the timings might be /mismatched with the names for a frame or two in
	// rare occassions, but I guess it wouldn't ever really matter.

	uint32_t regions_count = r1->next_region_index;

	r1_stats_t result = {
		.timings_count = regions_count,
		.timings       = m_alloc_array_nozero(arena, regions_count, r1_timing_t),
	};

	uint64_t timestamp_frequency = r1->timestamp_frequency;

	uint64_t *timestamps = rhi_begin_read_timestamps();
	for (size_t i = 0; i < regions_count; i++)
	{
		uint64_t start   = timestamps[2*i + 0];
		uint64_t end     = timestamps[2*i + 1];
		uint64_t delta   = end - start;
		double   seconds = (double)delta / (double)timestamp_frequency;

		r1_timing_t *timing = &result.timings[i];
		timing->identifier     = string_from_storage(r1->region_identifiers[i]);
		timing->inclusive_time = seconds;
		timing->exclusive_time = seconds; // TODO: Handle nested regions
	}
	rhi_end_read_timestamps();

	return result;
}

//------------------------------------------------------------------------

r1_view_t r1_create_view(rhi_window_t window)
{
	PROFILE_FUNC_BEGIN;

	r1_view_t                 view = {0};
	r1_view_render_targets_t *rts  = &view.targets;

	rts->rt_window = rhi_get_current_backbuffer(window);

	const rhi_texture_desc_t *desc = rhi_get_texture_desc(rts->rt_window);

	uint32_t w = view.render_w = desc->width;
	uint32_t h = view.render_h = desc->height;

	// TODO: My current implementation of one frame resources is very really bad
	// Wasting a lot of milliseconds on API abuse

	rts->depth_stencil = rhi_create_texture(&(rhi_create_texture_params_t){
		.debug_name        = S("ds_main"),
		.lifetime          = RhiResourceLifetime_one_frame,
		.dimension         = RhiTextureDimension_2d,
		.width             = w,
		.height            = h,
		.multisample_count = r1->multisample_count,
		.usage             = RhiTextureUsage_depth_stencil,
		.format            = PixelFormat_d24_unorm_s8_uint,
	});

	rts->rt_hdr = rhi_create_texture(&(rhi_create_texture_params_t){
		.debug_name        = S("rt_hdr"),
		.lifetime          = RhiResourceLifetime_one_frame,
		.dimension         = RhiTextureDimension_2d,
		.width             = w,
		.height            = h,
		.multisample_count = r1->multisample_count,
		.usage             = RhiTextureUsage_render_target,
		.format            = PixelFormat_r16g16b16a16_float,
	});

	rts->rt_hdr_resolved = rhi_create_texture(&(rhi_create_texture_params_t){
		.debug_name        = S("rt_hdr_resolved"),
		.lifetime          = RhiResourceLifetime_one_frame,
		.dimension         = RhiTextureDimension_2d,
		.width             = w,
		.height            = h,
		.usage             = RhiTextureUsage_render_target,
		.format            = PixelFormat_r16g16b16a16_float,
	});

	for (size_t i = 0; i < ARRAY_COUNT(rts->bloom_targets); i++)
	{
		uint32_t divisor = 2u << i;

		uint32_t rt_w = (w + divisor - 1) / divisor;
		uint32_t rt_h = (h + divisor - 1) / divisor;

		rts->bloom_targets[i] = rhi_create_texture(&(rhi_create_texture_params_t){
			.debug_name = Sf("rt_bloom[%zu]", i),
			.lifetime   = RhiResourceLifetime_one_frame,
			.dimension  = RhiTextureDimension_2d,
			.width      = rt_w,
			.height     = rt_h,
			.usage      = RhiTextureUsage_render_target,
			.format     = PixelFormat_r16g16b16a16_float,
		});
	}

	PROFILE_FUNC_END;

	return view;
}

void r1_init_map_resources(map_t *map)
{
	// TODO: I don't really know where I want to store resources like this. But I don't think r1 directly
	// should be concerned with it.

	uint32_t positions_size = (uint32_t)(sizeof(map->vertex.positions[0])*map->vertex_count);
	uint32_t uvs_size       = (uint32_t)(sizeof(map->vertex.texcoords[0])*map->vertex_count);
	uint32_t indices_size   = (uint32_t)(sizeof(map->indices[0])*map->index_count);

	r1->map.positions = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_positions"),
		.desc = {
			.first_element  = 0,
			.element_count  = map->vertex_count,
			.element_stride = sizeof(map->vertex.positions[0]),
		},
		.initial_data = {
			.src  = map->vertex.positions,
			.size = positions_size,
		},
	});

	r1->map.uvs = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_uvs"),
		.desc = {
			.first_element  = 0,
			.element_count  = map->vertex_count,
			.element_stride = sizeof(map->vertex.texcoords[0]),
		},
		.initial_data = {
			.src  = map->vertex.texcoords,
			.size = uvs_size,
		},
	});

	r1->map.lightmap_uvs = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_lightmap_uvs"),
		.desc = {
			.first_element  = 0,
			.element_count  = map->vertex_count,
			.element_stride = sizeof(map->vertex.lightmap_texcoords[0]),
		},
		.initial_data = {
			.src  = map->vertex.lightmap_texcoords,
			.size = uvs_size,
		},
	});

	r1->map.indices = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_indices"),
		.desc = {
			.first_element  = 0,
			.element_count  = map->index_count,
			.element_stride = sizeof(map->indices[0]),
		},
		.initial_data = {
			.src  = map->indices,
			.size = indices_size,
		},
	});
}

void r1_begin_frame(void)
{
	m_reset(r1->push_constants_arena);
	r1->next_region_index = 0;
	r1->frame_index += 1;
}

fn_local void r1_render_sun_shadows(rhi_command_list_t *list, map_t *map);
fn_local void r1_render_map        (rhi_command_list_t *list, r1_view_t *view, map_t *map);
fn_local void r1_post_process      (rhi_command_list_t *list, r1_view_t *view);
fn_local void r1_render_debug_lines(rhi_command_list_t *list, r1_view_t *view);

void r1_render_game_view(rhi_command_list_t *list, r1_view_t *view, map_t *map)
{
	PROFILE_FUNC_BEGIN;

	m4x4_t world_to_clip = mul(view->proj_matrix, view->view_matrix);

	v3_t sun_direction = view->scene.sun_direction;

	v3_t   sun_view_origin    = add(make_v3(0, 0, 0), mul(-256.0f, sun_direction));
	v3_t   sun_view_direction = negate(sun_direction);
	m4x4_t sun_view           = make_view_matrix(sun_view_origin, sun_view_direction, make_v3(0, 0, 1));
	m4x4_t sun_proj           = make_orthographic_matrix(2048, 2048, 512);
	m4x4_t sun_matrix         = mul(sun_proj, sun_view);

	const rhi_texture_desc_t *backbuffer_desc = rhi_get_texture_desc(view->targets.rt_window);

	set_view_parameters(list, &(view_parameters_t) {
		.world_to_clip            = world_to_clip,
		.view_to_clip             = view->proj_matrix,
		.world_to_view            = view->view_matrix,
		.sun_matrix               = sun_matrix,
		.sun_direction            = sun_direction,
		.sun_color                = view->scene.sun_color,
		.view_size                = make_v2((float)backbuffer_desc->width, (float)backbuffer_desc->height),
		.fog_density              = view->scene.fog_density,
		.fog_absorption           = view->scene.fog_absorption,
		.fog_scattering           = view->scene.fog_scattering,
		.fog_phase_k              = view->scene.fog_phase_k,
		.fog_ambient_inscattering = view->scene.fog_ambient_inscattering,
		.frame_index              = r1->frame_index,
		.refresh_rate             = 240, // TODO: don't hardcore
	});

	if (map)
	{
		r1_render_sun_shadows(list, map);
		r1_render_map        (list, view, map);

		if (r1->debug_drawing_enabled)
		{
			r1_render_debug_lines(list, view);
		}
	}
	else
	{
		log(Renderer, Warning, "No map passed to r1_render_game_view");
	}

	r1_post_process(list, view);

	PROFILE_FUNC_END;
}

void r1_render_sun_shadows(rhi_command_list_t *list, map_t *map)
{
	PROFILE_FUNC_BEGIN;

	R1_TIMED_REGION(list, S("Sun Shadows"))
	{
		rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
			.depth_stencil = {
				.texture     = r1->shadow_map,
				.op          = RhiPassOp_clear,
				.clear_value = 0.0f,
			},
			.topology = RhiPrimitiveTopology_trianglelist,
			.viewport = {
				.width  = r1->shadow_map_resolution,
				.height = r1->shadow_map_resolution,
				.min_depth = 0.0f,
				.max_depth = 1.0f,
			},
			.scissor_rect = r1->unbounded_scissor_rect,
		});

		shadow_draw_parameters_t draw_parameters = {
			.positions = rhi_get_buffer_srv(r1->map.positions),
		};
		rhi_set_parameters(list, R1ParameterSlot_draw, &draw_parameters, sizeof(draw_parameters));

		r1_set_pso      (list, DfPso_sun_shadows);
		rhi_draw_indexed(list, r1->map.indices, map->index_count, 0, 0);

		rhi_graphics_pass_end(list);
	}

	PROFILE_FUNC_END;
}

void r1_render_map(rhi_command_list_t *list, r1_view_t *view, map_t *map)
{
	PROFILE_FUNC_BEGIN;

	R1_TIMED_REGION(list, S("Map"))
	{
		rhi_texture_t rt = view->targets.rt_hdr;

		const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(rt);
		const float w = (float)rt_desc->width;
		const float h = (float)rt_desc->height;

		rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
			.render_targets[0] = {
				.texture = rt,
			},
			.depth_stencil = {
				.texture     = view->targets.depth_stencil,
				.op          = RhiPassOp_clear,
				.clear_value = 0.0f,
			},
			.topology = RhiPrimitiveTopology_trianglelist,
			.viewport = { 0, 0, w, h, 0, 1 },
			.scissor_rect = r1->unbounded_scissor_rect,
		});

		r1_set_pso(list, DfPso_brush);

		brush_pass_parameters_t pass_parameters = {
			.positions     = rhi_get_buffer_srv (r1->map.positions),
			.uvs           = rhi_get_buffer_srv (r1->map.uvs),
			.lm_uvs        = rhi_get_buffer_srv (r1->map.lightmap_uvs),
			.sun_shadowmap = rhi_get_texture_srv(r1->shadow_map),
		};
		shader_brush_set_pass_params(list, &pass_parameters);

		for (size_t i = 0; i < map->poly_count; i++)
		{
			map_poly_t *poly = &map->polys[i];

			// epic disaster!
			// epic disaster!
			// epic disaster!

			rhi_texture_srv_t albedo_srv = r1->missing_texture_srv;

			asset_image_t *albedo = get_image(poly->texture);

			if (albedo != &missing_image)
			{
				if (rhi_texture_upload_complete(albedo->rhi_texture))
				{
					albedo_srv = rhi_get_texture_srv(albedo->rhi_texture);
				}
			}

			rhi_texture_srv_t lightmap_srv = r1->white_texture_srv;
			v2_t lightmap_dim = { 1.0f, 1.0f };

			if (RESOURCE_HANDLE_VALID(poly->lightmap_rhi))
			{
				lightmap_srv = rhi_get_texture_srv(poly->lightmap_rhi);

				const rhi_texture_desc_t *desc = rhi_get_texture_desc(poly->lightmap_rhi);
				lightmap_dim = make_v2((float)desc->width, (float)desc->height);
			}

			brush_draw_parameters_t draw_parameters = {
				.albedo        = albedo_srv,
				.lightmap      = lightmap_srv,
				.normal        = poly->normal,
			};
			shader_brush_set_draw_params(list, &draw_parameters);

			rhi_draw_indexed(list, r1->map.indices, poly->index_count, poly->first_index, 0);
		}

		rhi_graphics_pass_end(list);
	}

	PROFILE_FUNC_END;
}

void r1_render_debug_lines(rhi_command_list_t *list, r1_view_t *view)
{
	PROFILE_FUNC_BEGIN;

	if (debug_line_count > 0)
	{
		size_t upload_size = sizeof(debug_line_t)*debug_line_count;
		rhi_upload_buffer_data(r1->debug_lines, 0, debug_lines, upload_size, RhiUploadFreq_frame);

		R1_TIMED_REGION(list, S("Draw Debug Lines"))
		{
			rhi_simple_graphics_pass_begin(list, view->targets.rt_hdr, view->targets.depth_stencil, RhiPrimitiveTopology_linelist);
			{
				r1_set_pso(list, DfPso_debug_lines);

				debug_lines_draw_parameters_t draw_parameters = {
					.lines = rhi_get_buffer_srv(r1->debug_lines),
				};
				shader_debug_lines_set_draw_params(list, &draw_parameters);

				rhi_draw(list, 2*debug_line_count, 0);
			}
			rhi_graphics_pass_end(list);
		}
	}

	debug_line_count = 0;

	PROFILE_FUNC_END;
}

void r1_post_process(rhi_command_list_t *list, r1_view_t *view)
{
	PROFILE_FUNC_BEGIN;

	R1_TIMED_REGION(list, S("Resolve MSAA"))
	{
		rhi_simple_graphics_pass_begin(list, view->targets.rt_hdr_resolved, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
		{
			r1_set_pso(list, DfPso_resolve_msaa);

			uint32_t blue_noise_index = r1->frame_index % ARRAY_COUNT(r1->blue_noise);

			post_draw_parameters_t draw_parameters = {
				.hdr_color    = rhi_get_texture_srv(view->targets.rt_hdr),
				.depth_buffer = rhi_get_texture_srv(view->targets.depth_stencil),
				.shadow_map   = rhi_get_texture_srv(r1->shadow_map),
				.blue_noise   = r1->blue_noise_srv[blue_noise_index],
				.sample_count = r1->multisample_count,
			};
			shader_post_set_draw_params(list, &draw_parameters);

			rhi_draw(list, 3, 0);
		}
		rhi_graphics_pass_end(list);
	}

	size_t downsample_steps = (size_t)cvar_read_i32(&cvar_r1_bloom_downsample_steps);
	size_t upsample_steps   = (size_t)cvar_read_i32(&cvar_r1_bloom_upsample_steps);

	R1_TIMED_REGION(list, S("Bloom Generation"))
	{
		rhi_texture_t bloom_in = view->targets.rt_hdr_resolved;

		upsample_steps = MIN(upsample_steps, downsample_steps);

		for (size_t i = 0; i < downsample_steps; i++)
		{
			rhi_texture_t bloom_rt = view->targets.bloom_targets[i];

			const rhi_texture_desc_t *in_desc = rhi_get_texture_desc(bloom_in);

			v2_t in_size     = make_v2((float)in_desc->width, (float)in_desc->height);
			v2_t in_size_inv = div(1.0f, in_size);

			rhi_simple_graphics_pass_begin(list, bloom_rt, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
			{
				r1_set_pso(list, DfPso_bloom_blur);

				bloom_draw_parameters_t draw_parameters = {
					.tex_color    = rhi_get_texture_srv(bloom_in),
					.inv_tex_size = in_size_inv,
				};
				shader_bloom_set_draw_params(list, &draw_parameters);

				rhi_draw(list, 3, 0);
			}
			rhi_graphics_pass_end(list);

			bloom_in = bloom_rt;
		}

		for (size_t j = 0; j < upsample_steps; j++)
		{
			size_t i = downsample_steps - 1 - j;

			rhi_texture_t bloom_rt = view->targets.bloom_targets[i];

			const rhi_texture_desc_t *in_desc = rhi_get_texture_desc(bloom_in);

			v2_t in_size     = make_v2((float)in_desc->width, (float)in_desc->height);
			v2_t in_size_inv = div(1.0f, in_size);

			rhi_simple_graphics_pass_begin(list, bloom_rt, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
			{
				r1_set_pso(list, DfPso_bloom_blur);

				bloom_draw_parameters_t draw_parameters = {
					.tex_color    = rhi_get_texture_srv(bloom_in),
					.inv_tex_size = in_size_inv,
				};
				shader_bloom_set_draw_params(list, &draw_parameters);

				rhi_draw(list, 3, 0);
			}
			rhi_graphics_pass_end(list);

			bloom_in = bloom_rt;
		}
	}

	R1_TIMED_REGION(list, S("Post Process"))
	{
		rhi_simple_graphics_pass_begin(list, view->targets.rt_window, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
		{
			r1_set_pso(list, DfPso_post_process);

			uint32_t blue_noise_index = r1->frame_index % ARRAY_COUNT(r1->blue_noise);

			float bloom_amount = cvar_read_f32(&cvar_r1_bloom_amount);

			if (cvar_read_bool(&cvar_r1_bloom_preview))
			{
				bloom_amount = 1.0;
			}

			post_draw_parameters_t draw_parameters = {
				.blue_noise     = r1->blue_noise_srv[blue_noise_index],
				.resolved_color = rhi_get_texture_srv(view->targets.rt_hdr_resolved),
				.bloom0         = rhi_get_texture_srv(view->targets.bloom_targets[downsample_steps - upsample_steps]),
				.bloom_amount   = bloom_amount,
			};
			shader_post_set_draw_params(list, &draw_parameters);

			rhi_draw(list, 3, 0);
		}
		rhi_graphics_pass_end(list);
	}

	PROFILE_FUNC_END;
}

void r1_render_ui(rhi_command_list_t *list, r1_view_t *view, ui_render_command_list_t *ui_list)
{
	PROFILE_FUNC_BEGIN;

	if (ui_list->count == 0)
	{
		return;
	}

	rhi_buffer_t ui_rects = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("ui_rects"),
		.lifetime   = RhiResourceLifetime_one_frame,
		.desc = {
			.first_element  = 0,
			.element_count  = ui_list->count,
			.element_stride = sizeof(r_ui_rect_t),
		},
	});

	size_t upload_size = sizeof(r_ui_rect_t) * ui_list->count;

	if (upload_size > 0)
	{
		r_ui_rect_t *rects = rhi_begin_buffer_upload(ui_rects, 0, upload_size, RhiUploadFreq_frame);
		{
			for (size_t key_index = 0; key_index < ui_list->count; key_index++)
			{
				const ui_render_command_key_t *key = &ui_list->keys[key_index];

				const size_t command_index = key->command_index;
				const ui_render_command_t *command = &ui_list->commands[command_index];

				copy_memory(&rects[key_index], &command->rect, sizeof(command->rect));

				if (command->rect.texture.index == 0)
				{
					rects[key_index].texture = r1->white_texture_srv;
				}
			}
		}
		rhi_end_buffer_upload(ui_rects);
	}

	R1_TIMED_REGION(list, S("Draw UI"))
	{
		rhi_simple_graphics_pass_begin(list, view->targets.rt_window, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglestrip);
		{
			r1_set_pso(list, DfPso_ui);

			ui_draw_parameters_t draw_parameters = {
				.rects = rhi_get_buffer_srv(ui_rects),
			};
			shader_ui_set_draw_params(list, &draw_parameters);

			rhi_draw_instanced(list, 4, 0, (uint32_t)ui_list->count, 0);
		}
		rhi_graphics_pass_end(list);
	}

	if (cvar_read_bool(&cvar_r1_ui_heat_map))
	{
		// render heatmap

		rhi_texture_t rt_heatmap = rhi_create_texture(&(rhi_create_texture_params_t){
			.debug_name        = S("rt_ui_heatmap"),
			.lifetime          = RhiResourceLifetime_one_frame,
			.dimension         = RhiTextureDimension_2d,
			.width             = view->render_w,
			.height            = view->render_h,
			.usage             = RhiTextureUsage_render_target,
			.format            = PixelFormat_r8g8b8a8_unorm,
		});

		rhi_begin_region(list, S("UI Heatmap"));

		const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(rt_heatmap);

		rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
			.render_targets[0] = { 
				.texture = rt_heatmap,
				.op = RhiPassOp_clear,
			},
			.topology = RhiPrimitiveTopology_trianglestrip,
			.viewport = {
				.width  = (float)rt_desc->width,
				.height = (float)rt_desc->height,
				.min_depth = 0.0f,
				.max_depth = 1.0f,
			},
			.scissor_rect = {
				.max = { (int32_t)rt_desc->width, (int32_t)rt_desc->height },
			},
		});
		{
			r1_set_pso(list, DfPso_ui_heatmap);

			ui_draw_parameters_t draw_parameters = {
				.rects = rhi_get_buffer_srv(ui_rects),
			};
			shader_ui_set_draw_params(list, &draw_parameters);

			rhi_draw_instanced(list, 4, 0, (uint32_t)ui_list->count, 0);
		}
		rhi_graphics_pass_end(list);

		// visualize heatmap
		rhi_simple_graphics_pass_begin(list, view->targets.rt_window, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
		{
			r1_set_pso(list, DfPso_ui_visualize_heatmap);

			int32_t heat_map_scale = cvar_read_i32(&cvar_r1_ui_heat_map_scale);

			ui_visualize_heatmap_draw_parameters_t draw_parameters = {
				.heatmap = rhi_get_texture_srv(rt_heatmap),
				.scale   = 255.0f / (float)heat_map_scale,
			};
			shader_ui_visualize_heatmap_set_draw_params(list, &draw_parameters);

			rhi_draw(list, 3, 0);
		}
		rhi_graphics_pass_end(list);

		rhi_end_region(list);
	}

	PROFILE_FUNC_END;
}

void draw_debug_line(v3_t start, v3_t end, v4_t start_color, v4_t end_color)
{
	if (ALWAYS(debug_line_count < ARRAY_COUNT(debug_lines)))
	{
		debug_line_t *line = &debug_lines[debug_line_count++];
		line->start       = start;
		line->end         = end;
		line->start_color = pack_color(linear_to_srgb(start_color));
		line->end_color   = pack_color(linear_to_srgb(end_color));
	}
}

void draw_debug_cube(rect3_t bounds, v4_t color)
{
    v3_t v000 = { bounds.min.x, bounds.min.y, bounds.min.z };
    v3_t v100 = { bounds.max.x, bounds.min.y, bounds.min.z };
    v3_t v010 = { bounds.min.x, bounds.max.y, bounds.min.z };
    v3_t v110 = { bounds.max.x, bounds.max.y, bounds.min.z };

    v3_t v001 = { bounds.min.x, bounds.min.y, bounds.max.z };
    v3_t v101 = { bounds.max.x, bounds.min.y, bounds.max.z };
    v3_t v011 = { bounds.min.x, bounds.max.y, bounds.max.z };
    v3_t v111 = { bounds.max.x, bounds.max.y, bounds.max.z };

    // bottom plane
    draw_debug_line(v000, v100, color, color);
    draw_debug_line(v100, v110, color, color);
    draw_debug_line(v110, v010, color, color);
    draw_debug_line(v010, v000, color, color);

    // top plane
    draw_debug_line(v001, v101, color, color);
    draw_debug_line(v101, v111, color, color);
    draw_debug_line(v111, v011, color, color);
    draw_debug_line(v011, v001, color, color);

    // "pillars"
    draw_debug_line(v000, v001, color, color);
    draw_debug_line(v100, v101, color, color);
    draw_debug_line(v010, v011, color, color);
    draw_debug_line(v110, v111, color, color);
}

void draw_oriented_debug_cube(v3_t p, v3_t dim, quat_t rot, v4_t color)
{
    v3_t v000 = { -dim.x, -dim.y, -dim.z };
    v3_t v100 = {  dim.x, -dim.y, -dim.z };
    v3_t v010 = { -dim.x,  dim.y, -dim.z };
    v3_t v110 = {  dim.x,  dim.y, -dim.z };

    v3_t v001 = { -dim.x, -dim.y,  dim.z };
    v3_t v101 = {  dim.x, -dim.y,  dim.z };
    v3_t v011 = { -dim.x,  dim.y,  dim.z };
    v3_t v111 = {  dim.x,  dim.y,  dim.z };

	v000 = add(p, quat_rotatev(rot, v000));
	v100 = add(p, quat_rotatev(rot, v100));
	v010 = add(p, quat_rotatev(rot, v010));
	v110 = add(p, quat_rotatev(rot, v110));

	v001 = add(p, quat_rotatev(rot, v001));
	v101 = add(p, quat_rotatev(rot, v101));
	v011 = add(p, quat_rotatev(rot, v011));
	v111 = add(p, quat_rotatev(rot, v111));

    // bottom plane
    draw_debug_line(v000, v100, color, color);
    draw_debug_line(v100, v110, color, color);
    draw_debug_line(v110, v010, color, color);
    draw_debug_line(v010, v000, color, color);

    // top plane
    draw_debug_line(v001, v101, color, color);
    draw_debug_line(v101, v111, color, color);
    draw_debug_line(v111, v011, color, color);
    draw_debug_line(v011, v001, color, color);

    // "pillars"
    draw_debug_line(v000, v001, color, color);
    draw_debug_line(v100, v101, color, color);
    draw_debug_line(v010, v011, color, color);
    draw_debug_line(v110, v111, color, color);
}

//------------------------------------------------------------------------
// Draw stream API sketch:

typedef struct r1_draw_stream_t
{
	uint32_t           capacity;
	uint32_t           count;
	uint32_t          *sort_keys;
	rhi_draw_packet_t *packets;
} r1_draw_stream_t;

fn void r1_push_draw_packet(r1_draw_stream_t *stream, rhi_draw_packet_t packet, uint32_t sort_key)
{
	if (ALWAYS(stream->count < stream->capacity))
	{
		uint32_t index = stream->count++;

		stream->sort_keys[index] = sort_key;
		stream->packets  [index] = packet;
	}
}

typedef struct view_t
{
	uint32_t         params_offset;
	r1_draw_stream_t buckets[R1RenderBucket_COUNT];
} view_t;

void draw_map(map_t *map, int view_count, view_t *views)
{
	rhi_parameters_t pass_alloc = rhi_alloc_parameters(sizeof(brush_pass_parameters_t));

	brush_pass_parameters_t *pass = pass_alloc.host;
	pass->positions     = rhi_get_buffer_srv (r1->map.positions);
	pass->uvs           = rhi_get_buffer_srv (r1->map.uvs);
	pass->lm_uvs        = rhi_get_buffer_srv (r1->map.lightmap_uvs);
	pass->sun_shadowmap = rhi_get_texture_srv(r1->shadow_map);

	for (size_t i = 0; i < map->poly_count; i++)
	{
		map_plane_t *plane = &map->planes[i];
		map_poly_t  *poly  = &map->polys [i];

		for (int view_index = 0; view_index < view_count; view_index++)
		{
			view_t *view = &views[view_index];

			bool is_water = !!(plane->content_flags & MapContentFlag_water);

			r1_draw_stream_t *stream = &view->buckets[is_water ? R1RenderBucket_translucent : R1RenderBucket_opaque];

			// epic disaster!
			// epic disaster!
			// epic disaster!

			rhi_texture_srv_t albedo_srv = r1->white_texture_srv;

			asset_image_t *albedo = get_image(poly->texture);

			if (albedo != &missing_image)
			{
				if (rhi_texture_upload_complete(albedo->rhi_texture))
				{
					albedo_srv = rhi_get_texture_srv(albedo->rhi_texture);
				}
			}

			rhi_texture_srv_t lightmap_srv = r1->white_texture_srv;
			v2_t lightmap_dim = { 1.0f, 1.0f };

			if (RESOURCE_HANDLE_VALID(poly->lightmap_rhi))
			{
				lightmap_srv = rhi_get_texture_srv(poly->lightmap_rhi);

				const rhi_texture_desc_t *desc = rhi_get_texture_desc(poly->lightmap_rhi);
				lightmap_dim = make_v2((float)desc->width, (float)desc->height);
			}

			// ~~~

			brush_draw_parameters_t *draw = m_alloc_struct_nozero(r1->push_constants_arena, brush_draw_parameters_t);
			draw->albedo   = albedo_srv;
			draw->lightmap = lightmap_srv;
			draw->normal   = poly->normal;

			rhi_indirect_draw_indexed_t *args = NULL; // r1_alloc_args();
			args->index_count  = poly->index_count;
			args->index_offset = poly->first_index;

			uint32_t sort_key = 0;

			rhi_draw_packet_t packet = {
				.pso            = r1->psos[DfPso_brush], //r1->psos.map,
				.args_buffer    = r1->args,
				.args_offset    = 0, // r1_get_args_offset(args);
				.index_buffer   = r1->map.indices,
				.push_constants = m_get_alloc_offset(r1->push_constants_arena, draw),
				.params[R1ParameterSlot_view] = view->params_offset,
				.params[R1ParameterSlot_pass] = pass_alloc.offset,
			};
			r1_push_draw_packet(stream, packet, sort_key);
		}
	}
}