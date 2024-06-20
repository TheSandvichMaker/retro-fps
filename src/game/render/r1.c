#include "r1.h"

#include "shaders/gen/shaders.c"

CVAR_BOOL(cvar_r1_ui_heat_map, "r1.ui.heat_map", true);

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

fn_local void r1_create_psos(uint32_t multisample_count)
{
	rhi_destroy_pso(r1->psos.map);
	rhi_destroy_pso(r1->psos.sun_shadows);
	rhi_destroy_pso(r1->psos.debug_lines);
	rhi_destroy_pso(r1->psos.post_process);
	rhi_destroy_pso(r1->psos.ui);

	const bool multisample_enabled = multisample_count > 1;

	// shadow
	{
		df_shader_info_t *vs = &df_shaders[DfShader_shadow_vs];

		r1->psos.sun_shadows = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_shadow_map"),
			.vs = vs->bytecode,
			.blend.sample_mask = 0xFFFFFFFF,
			.rasterizer = {
				.cull_mode     = RhiCullMode_back,
				.front_winding = RhiWinding_ccw,
			},
			.depth_stencil = {
				.depth_test_enable = true,
				.depth_write       = true,
				.depth_func        = RhiComparisonFunc_greater,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.dsv_format              = PixelFormat_d24_unorm_s8_uint,
		});
	}

	// brush
	{
		df_shader_info_t *vs = &df_shaders[DfShader_brush_vs];
		df_shader_info_t *ps = &df_shaders[DfShader_brush_ps];

		r1->psos.map = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_brush"),
			.vs = vs->bytecode,
			.ps = ps->bytecode,
			.blend = {
				.render_target[0].write_mask = RhiColorWriteEnable_all,
				.sample_mask                 = 0xFFFFFFFF,
			},
			.rasterizer = {
				.cull_mode          = RhiCullMode_back,
				.front_winding      = RhiWinding_ccw,
				.multisample_enable = multisample_enabled,
			},
			.depth_stencil = {
				.depth_test_enable = true,
				.depth_write       = true,
				.depth_func        = RhiComparisonFunc_greater,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.multisample_count       = multisample_count,
			.rtv_formats[0]          = PixelFormat_r16g16b16a16_float,
			.dsv_format              = PixelFormat_d24_unorm_s8_uint,
		});
	}

	// debug lines
	{
		df_shader_info_t *vs = &df_shaders[DfShader_debug_lines_vs];
		df_shader_info_t *ps = &df_shaders[DfShader_debug_lines_ps];

		r1->psos.debug_lines = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_debug_lines"),
			.vs = vs->bytecode,
			.ps = ps->bytecode,
			.blend = {
				.render_target[0].write_mask = RhiColorWriteEnable_all,
				.sample_mask                 = 0xFFFFFFFF,
			},
			.rasterizer = {
				.cull_mode          = RhiCullMode_back,
				.front_winding      = RhiWinding_ccw,
				.multisample_enable = multisample_enabled,
			},
			.depth_stencil = {
				.depth_test_enable = true,
				.depth_func        = RhiComparisonFunc_greater,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_line,
			.render_target_count     = 1,
			.multisample_count       = multisample_count,
			.rtv_formats[0]          = PixelFormat_r16g16b16a16_float,
			.dsv_format              = PixelFormat_d24_unorm_s8_uint,
		});
	}

	// post process
	{
		df_shader_info_t *vs = &df_shaders[DfShader_fullscreen_triangle_vs];
		df_shader_info_t *ps = &df_shaders[DfShader_post_ps];

		r1->psos.post_process = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_post_process"),
			.vs = vs->bytecode,
			.ps = ps->bytecode,
			.blend = {
				.render_target[0].write_mask = RhiColorWriteEnable_all,
				.sample_mask                 = 0xFFFFFFFF,
			},
			.rasterizer = {
				.cull_mode     = RhiCullMode_back,
				.front_winding = RhiWinding_ccw,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = PixelFormat_r8g8b8a8_unorm_srgb,
		});
	}

	// ui
	{
		df_shader_info_t *vs = &df_shaders[DfShader_ui_vs];
		df_shader_info_t *ps = &df_shaders[DfShader_ui_ps];

		r1->psos.ui = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_ui"),
			.vs = vs->bytecode,
			.ps = ps->bytecode,
			.blend = {
				.render_target[0] = rhi_blend_premul_alpha(),
				.sample_mask = 0xFFFFFFFF,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = PixelFormat_r8g8b8a8_unorm_srgb,
		});
	}

	// ui heatmap
	{
		df_shader_info_t *vs = &df_shaders[DfShader_ui_vs];
		df_shader_info_t *ps = &df_shaders[DfShader_ui_heatmap_ps];

		r1->psos.ui_heatmap = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_ui_heatmap"),
			.vs = vs->bytecode,
			.ps = ps->bytecode,
			.blend = {
				.render_target[0] = rhi_blend_additive(),
				.sample_mask = 0xFFFFFFFF,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = PixelFormat_r8g8b8a8_unorm,
		});
	}

	// visualize ui heatmap
	{
		df_shader_info_t *vs = &df_shaders[DfShader_fullscreen_triangle_vs];
		df_shader_info_t *ps = &df_shaders[DfShader_ui_visualize_heatmap_ps];

		r1->psos.ui_visualize_heatmap = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_ui_visualize_heatmap"),
			.vs = vs->bytecode,
			.ps = ps->bytecode,
			.blend = {
				.render_target[0] = rhi_blend_premul_alpha(),
				.sample_mask                 = 0xFFFFFFFF,
			},
			.rasterizer = {
				.cull_mode     = RhiCullMode_back,
				.front_winding = RhiWinding_ccw,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = PixelFormat_r8g8b8a8_unorm_srgb,
		});
	}
}

fn_local void r1_load_all_shaders(void);

void r1_init(void)
{
	ASSERT(!r1);

	r1 = m_bootstrap(r1_state_t, arena);
	r1->debug_drawing_enabled = true;

	r1->push_constants_arena = m_child_arena(&r1->arena, MB(2));

	r1_load_all_shaders();

	const uint32_t max_timestamps = 2*R1MaxRegions; // one for region start, one for region end

	// the RHI layer doesn't create the timestamp query/readback buffers automatically,
	// because you might not care and you want to be able to specify the capacity in
	// application logic I think
	rhi_init_timestamps(max_timestamps);
	r1->timestamp_frequency = rhi_get_timestamp_frequency();

	r1->multisample_count = 8;

	uint32_t white_pixel = 0xFFFFFFFF;

	r1->white_texture = rhi_create_texture(&(rhi_create_texture_params_t){
	    .debug_name = S("builtin_white_texture"),
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

	uint32_t black_pixel = 0x0;

	r1->black_texture = rhi_create_texture(&(rhi_create_texture_params_t){
	    .debug_name = S("builtin_black_texture"),
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

	r1->shadow_map_resolution = 2048;
	r1->shadow_map = rhi_create_texture(&(rhi_create_texture_params_t){
	    .debug_name = S("sun_shadow_map"),
		.dimension  = RhiTextureDimension_2d,
		.width      = r1->shadow_map_resolution,
		.height     = r1->shadow_map_resolution,
		.usage      = RhiTextureUsage_depth_stencil,
		.format     = PixelFormat_d24_unorm_s8_uint,
	});

	r1->unbounded_scissor_rect = (rect2i_t){ .min = { 0, 0 }, .max = { INT32_MAX, INT32_MAX } };

	r1->ui_rects = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("ui_rects"),
		.desc = {
			.first_element  = 0,
			.element_count  = R1MaxUiRects,
			.element_stride = sizeof(r_ui_rect_t),
		},
		.flags = RhiResourceFlag_dynamic,
	});

	r1->debug_lines = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("debug_lines"),
		.desc = {
			.first_element  = 0,
			.element_count  = (uint32_t)ARRAY_COUNT(debug_lines),
			.element_stride = sizeof(debug_line_t),
		},
	});

	r1_create_psos(r1->multisample_count);

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
}

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

void r1_init_map_resources(map_t *map)
{
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

void r1_update_window_resources(rhi_window_t window)
{
	rhi_texture_t rt = rhi_get_current_backbuffer(window);

	const rhi_texture_desc_t *desc = rhi_get_texture_desc(rt);

	if (r1->window.width  != desc->width ||
		r1->window.height != desc->height)
	{
		r1->window.width  = desc->width;
		r1->window.height = desc->height;

		rhi_recreate_texture(&r1->window.depth_stencil, &(rhi_create_texture_params_t){
			.debug_name        = S("window_depth_buffer"),
			.dimension         = RhiTextureDimension_2d,
			.width             = r1->window.width,
			.height            = r1->window.height,
			.multisample_count = r1->multisample_count,
			.usage             = RhiTextureUsage_depth_stencil,
			.format            = PixelFormat_d24_unorm_s8_uint,
		});

		rhi_recreate_texture(&r1->window.rt_hdr, &(rhi_create_texture_params_t){
			.debug_name        = S("window_hdr_target"),
			.dimension         = RhiTextureDimension_2d,
			.width             = r1->window.width,
			.height            = r1->window.height,
			.multisample_count = r1->multisample_count,
			.usage             = RhiTextureUsage_render_target,
			.format            = PixelFormat_r16g16b16a16_float,
		});

		rhi_recreate_texture(&r1->window.ui_heatmap_rt, &(rhi_create_texture_params_t){
			.debug_name        = S("ui_heatmap_rt"),
			.dimension         = RhiTextureDimension_2d,
			.width             = r1->window.width,
			.height            = r1->window.height,
			.multisample_count = 1,
			.usage             = RhiTextureUsage_render_target,
			.format            = PixelFormat_r8g8b8a8_unorm,
		});
	}
}

void r1_begin_frame(void)
{
	m_reset(r1->push_constants_arena);
	r1->next_region_index = 0;
}

void r1_finish_recording_draw_streams(void)
{
}

fn_local void r1_render_sun_shadows(rhi_command_list_t *list, map_t *map);
fn_local void r1_render_map        (rhi_command_list_t *list, rhi_texture_t rt, map_t *map);
fn_local void r1_post_process      (rhi_command_list_t *list, rhi_texture_t color_hdr, rhi_texture_t rt_result);
fn_local void r1_render_debug_lines(rhi_command_list_t *list, rhi_texture_t rt, rhi_texture_t rt_depth);

void r1_render_game_view(rhi_command_list_t *list, rhi_texture_t backbuffer, r_view_t *view, map_t *map)
{
	R1_TIMED_REGION(list, S("Game View"))
	{
		m4x4_t world_to_clip = mul(view->proj_matrix, view->view_matrix);

		v3_t sun_direction = view->scene.sun_direction;

		v3_t   sun_view_origin    = add(make_v3(0, 0, 0), mul(-256.0f, sun_direction));
		v3_t   sun_view_direction = negate(sun_direction);
		m4x4_t sun_view           = make_view_matrix(sun_view_origin, sun_view_direction, make_v3(0, 0, 1));
		m4x4_t sun_proj           = make_orthographic_matrix(2048, 2048, 512);
		m4x4_t sun_matrix         = mul(sun_proj, sun_view);

		const rhi_texture_desc_t *backbuffer_desc = rhi_get_texture_desc(backbuffer);

		set_view_parameters(list, &(view_parameters_t) {
			.world_to_clip = world_to_clip,
			.sun_matrix    = sun_matrix,
			.sun_direction = sun_direction,
			.sun_color     = view->scene.sun_color,
			.view_size     = make_v2((float)backbuffer_desc->width, (float)backbuffer_desc->height),
		});

		rhi_texture_t rt_hdr = r1->window.rt_hdr;

		if (map)
		{
			r1_render_sun_shadows(list, map);
			r1_render_map        (list, rt_hdr, map);

			if (r1->debug_drawing_enabled)
			{
				r1_render_debug_lines(list, rt_hdr, r1->window.depth_stencil);
			}
		}

		// r1_post_process(list, rt_hdr, backbuffer);
		rhi_do_test_stuff(list, rt_hdr, backbuffer, r1->psos.post_process);
	}
}

void r1_render_sun_shadows(rhi_command_list_t *list, map_t *map)
{
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

		rhi_set_pso     (list, r1->psos.sun_shadows);
		rhi_draw_indexed(list, r1->map.indices, map->index_count, 0, 0);

		rhi_graphics_pass_end(list);
	}
}

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
				.pso            = r1->psos.map,
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

void r1_render_map(rhi_command_list_t *list, rhi_texture_t rt, map_t *map)
{
	R1_TIMED_REGION(list, S("Map"))
	{
		const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(rt);
		const float w = (float)rt_desc->width;
		const float h = (float)rt_desc->height;

		rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
			.render_targets[0] = {
				.texture     = rt,
				.op          = RhiPassOp_clear,
				.clear_color = make_v4(0.05f, 0.15f, 0.05f, 1.0f),
			},
			.depth_stencil = {
				.texture     = r1->window.depth_stencil,
				.op          = RhiPassOp_clear,
				.clear_value = 0.0f,
			},
			.topology = RhiPrimitiveTopology_trianglelist,
			.viewport = { 0, 0, w, h, 0, 1 },
			.scissor_rect = r1->unbounded_scissor_rect,
		});

		rhi_set_pso(list, r1->psos.map);

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
}

void r1_render_debug_lines(rhi_command_list_t *list, rhi_texture_t rt, rhi_texture_t rt_depth)
{
	if (debug_line_count > 0)
	{
		size_t upload_size = sizeof(debug_line_t)*debug_line_count;
		rhi_upload_buffer_data(r1->debug_lines, 0, debug_lines, upload_size, RhiUploadFreq_frame);

		R1_TIMED_REGION(list, S("Draw Debug Lines"))
		{
			rhi_simple_graphics_pass_begin(list, rt, rt_depth, RhiPrimitiveTopology_linelist);
			{
				rhi_set_pso(list, r1->psos.debug_lines);

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
}

void r1_post_process(rhi_command_list_t *list, rhi_texture_t hdr_color, rhi_texture_t rt_result)
{
	R1_TIMED_REGION(list, S("Post Process"))
	{
		rhi_simple_graphics_pass_begin(list, rt_result, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
		{
			rhi_set_pso(list, r1->psos.post_process);

			post_draw_parameters_t draw_parameters = {
				.hdr_color    = rhi_get_texture_srv(hdr_color),
				.sample_count = r1->multisample_count,
			};
			shader_post_set_draw_params(list, &draw_parameters);

			rhi_draw(list, 3, 0);
		}
		rhi_graphics_pass_end(list);
	}
}

void r1_render_ui(rhi_command_list_t *list, rhi_texture_t rt, ui_render_command_list_t *ui_list)
{
	size_t upload_size = sizeof(r_ui_rect_t) * ui_list->count;

	if (upload_size > 0)
	{
		r_ui_rect_t *rects = rhi_begin_buffer_upload(r1->ui_rects, 0, upload_size, RhiUploadFreq_frame);
		{
			for (size_t key_index = 0; key_index < ui_list->count; key_index++)
			{
				const ui_render_command_key_t *key = &ui_list->keys[key_index];

				const size_t command_index = key->command_index;
				const ui_render_command_t *command = &ui_list->commands[command_index];

				memcpy(&rects[key_index], &command->rect, sizeof(command->rect));

				if (command->rect.texture.index == 0)
				{
					rects[key_index].texture = r1->white_texture_srv;
				}
			}
		}
		rhi_end_buffer_upload(r1->ui_rects);
	}

	R1_TIMED_REGION(list, S("Draw UI"))
	{
		rhi_simple_graphics_pass_begin(list, rt, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglestrip);
		{
			rhi_set_pso(list, r1->psos.ui);

			ui_draw_parameters_t draw_parameters = {
				.rects = rhi_get_buffer_srv(r1->ui_rects),
			};
			shader_ui_set_draw_params(list, &draw_parameters);

			rhi_draw_instanced(list, 4, 0, (uint32_t)ui_list->count, 0);
		}
		rhi_graphics_pass_end(list);
	}

	if (cvar_read_bool(&cvar_r1_ui_heat_map))
	{
		// render heatmap

		const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(r1->window.ui_heatmap_rt);

		rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
			.render_targets[0] = { 
				.texture = r1->window.ui_heatmap_rt,
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
			rhi_set_pso(list, r1->psos.ui_heatmap);

			ui_draw_parameters_t draw_parameters = {
				.rects = rhi_get_buffer_srv(r1->ui_rects),
			};
			shader_ui_set_draw_params(list, &draw_parameters);

			rhi_draw_instanced(list, 4, 0, (uint32_t)ui_list->count, 0);
		}
		rhi_graphics_pass_end(list);

		// visualize heatmap
		rhi_simple_graphics_pass_begin(list, rt, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
		{
			rhi_set_pso(list, r1->psos.ui_visualize_heatmap);

			ui_visualize_heatmap_draw_parameters_t draw_parameters = {
				.heatmap = rhi_get_texture_srv(r1->window.ui_heatmap_rt),
				.scale   = 255.0f / 16.0f,
			};
			shader_ui_visualize_heatmap_set_draw_params(list, &draw_parameters);

			rhi_draw(list, 3, 0);
		}
		rhi_graphics_pass_end(list);
	}
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
