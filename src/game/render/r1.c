typedef enum r_parameter_slot_t
{
	R1ParameterSlot_draw = 0,
	R1ParameterSlot_pass = 1,
	R1ParameterSlot_view = 2,
} r_parameter_slot;

typedef struct view_parameters_t
{
	m4x4_t world_to_clip;
	m4x4_t sun_matrix;
	alignas(16) v3_t sun_direction;
	alignas(16) v3_t sun_color;
	alignas(16) v2_t view_size;
} view_parameters_t;

typedef struct map_pass_parameters_t
{
	alignas(16) rhi_buffer_srv_t  positions;
	alignas(16) rhi_buffer_srv_t  uvs;
	alignas(16) rhi_buffer_srv_t  lightmap_uvs;
	alignas(16) rhi_texture_srv_t sun_shadowmap;
	alignas(16) v2_t              shadowmap_dim;
} map_pass_parameters_t;

typedef struct map_draw_parameters_t
{
	rhi_texture_srv_t albedo;
	v2_t              albedo_dim;

	alignas(16)
	rhi_texture_srv_t lightmap;
	v2_t              lightmap_dim;

	alignas(16)
	v3_t normal;
} map_draw_parameters_t;

typedef struct shadow_draw_parameters_t
{
	rhi_buffer_srv_t positions;
} shadow_draw_parameters_t; 

typedef struct post_process_draw_parameters_t
{
	rhi_texture_srv_t hdr_color;
	uint32_t          sample_count;
} post_process_draw_parameters_t;

typedef struct ui_draw_parameters_t
{
	rhi_buffer_srv_t rects;
} ui_draw_parameters_t;

typedef struct debug_line_draw_parameters_t
{
	rhi_buffer_srv_t lines;
} debug_line_draw_parameters_t; 

#define set_draw_parameters(list, parameters) rhi_set_parameters(list, R1ParameterSlot_draw, parameters, sizeof(*(parameters)))

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

	m_scoped_temp
	{
		string_t source = fs_read_entire_file(temp, S("../src/shaders/bindless_shadow.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp, source, S("bindless_shadow.hlsl"), S("MainVS"), S("vs_6_8"));

		r1->psos.sun_shadows = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.vs = vs,
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

	m_scoped_temp
	{
		string_t source = fs_read_entire_file(temp, S("../src/shaders/bindless_draft.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp, source, S("bindless_draft.hlsl"), S("MainVS"), S("vs_6_8"));
		rhi_shader_bytecode_t ps = rhi_compile_shader(temp, source, S("bindless_draft.hlsl"), S("MainPS"), S("ps_6_8"));

		r1->psos.map = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("Map PSO"),
			.vs = vs,
			.ps = ps,
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

	m_scoped_temp
	{
		string_t source = fs_read_entire_file(temp, S("../src/shaders/debug_lines.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp, source, S("debug_lines.hlsl"), S("MainVS"), S("vs_6_8"));
		rhi_shader_bytecode_t ps = rhi_compile_shader(temp, source, S("debug_lines.hlsl"), S("MainPS"), S("ps_6_8"));

		r1->psos.debug_lines = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("Debug Lines PSO"),
			.vs = vs,
			.ps = ps,
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

	m_scoped_temp
	{
		string_t source = fs_read_entire_file(temp, S("../src/shaders/bindless_post.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp, source, S("bindless_post.hlsl"), S("MainVS"), S("vs_6_8"));
		rhi_shader_bytecode_t ps = rhi_compile_shader(temp, source, S("bindless_post.hlsl"), S("MainPS"), S("ps_6_8"));

		r1->psos.post_process = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("Post Process PSO"),
			.vs = vs,
			.ps = ps,
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
			.rtv_formats[0]          = PixelFormat_r8g8b8a8_unorm,
		});
	}

	m_scoped_temp
	{
		string_t source = fs_read_entire_file(temp, S("../src/shaders/bindless_ui.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp, source, S("bindless_ui.hlsl"), S("MainVS"), S("vs_6_8"));
		rhi_shader_bytecode_t ps = rhi_compile_shader(temp, source, S("bindless_ui.hlsl"), S("MainPS"), S("ps_6_8"));

		r1->psos.ui = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("UI PSO"),
			.vs = vs,
			.ps = ps,
			.blend = {
				.render_target[0] = {
					.blend_enable    = true,
					.src_blend       = RhiBlend_src_alpha,
					.dst_blend       = RhiBlend_inv_src_alpha,
					.blend_op        = RhiBlendOp_add,
					.src_blend_alpha = RhiBlend_src_alpha,
					.dst_blend_alpha = RhiBlend_inv_src_alpha,
					.blend_op_alpha  = RhiBlendOp_add,
					.write_mask      = RhiColorWriteEnable_all,
				},
				.sample_mask = 0xFFFFFFFF,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = PixelFormat_r8g8b8a8_unorm,
		});
	}
}

void r1_init(void)
{
	ASSERT(!r1);

	r1 = m_bootstrap(r1_state_t, arena);
	r1->debug_drawing_enabled = true;

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

	r1->shadow_map_resolution = 1024;
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
		.size       = sizeof(r_ui_rect_t) * R1MaxUiRects,
		.flags      = RhiResourceFlag_frame_buffered,
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = R1MaxUiRects,
			.element_stride = sizeof(r_ui_rect_t),
		},
	});

	r1->debug_lines = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("debug_lines"),
		.size       = sizeof(debug_line_t)*ARRAY_COUNT(debug_lines),
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = (uint32_t)ARRAY_COUNT(debug_lines),
			.element_stride = sizeof(debug_line_t),
		},
	});

	r1_create_psos(r1->multisample_count);
}

void r1_init_map_resources(map_t *map)
{
	uint32_t positions_size = (uint32_t)(sizeof(map->vertex.positions[0])*map->vertex_count);
	uint32_t uvs_size       = (uint32_t)(sizeof(map->vertex.texcoords[0])*map->vertex_count);
	uint32_t indices_size   = (uint32_t)(sizeof(map->indices[0])*map->index_count);

	r1->map.positions = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_positions"),
		.size       = positions_size,
		.initial_data = {
			.ptr  = map->vertex.positions,
			.size = positions_size,
		},
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = map->vertex_count,
			.element_stride = sizeof(map->vertex.positions[0]),
		},
	});

	r1->map.uvs = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_uvs"),
		.size       = uvs_size,
		.initial_data = {
			.ptr  = map->vertex.texcoords,
			.size = uvs_size,
		},
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = map->vertex_count,
			.element_stride = sizeof(map->vertex.texcoords[0]),
		},
	});

	r1->map.lightmap_uvs = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_lightmap_uvs"),
		.size       = uvs_size,
		.initial_data = {
			.ptr  = map->vertex.lightmap_texcoords,
			.size = uvs_size,
		},
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = map->vertex_count,
			.element_stride = sizeof(map->vertex.lightmap_texcoords[0]),
		},
	});

	r1->map.indices = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("map_indices"),
		.size       = indices_size,
		.initial_data = {
			.ptr  = map->indices,
			.size = indices_size,
		},
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = map->index_count,
			.element_stride = sizeof(map->indices[0]),
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

		rhi_destroy_texture(r1->window.depth_stencil);

		r1->window.depth_stencil = rhi_create_texture(&(rhi_create_texture_params_t){
			.debug_name        = S("window_depth_buffer"),
			.dimension         = RhiTextureDimension_2d,
			.width             = r1->window.width,
			.height            = r1->window.height,
			.multisample_count = r1->multisample_count,
			.usage             = RhiTextureUsage_depth_stencil,
			.format            = PixelFormat_d24_unorm_s8_uint,
		});

		rhi_destroy_texture(r1->window.rt_hdr);

		r1->window.rt_hdr = rhi_create_texture(&(rhi_create_texture_params_t){
			.debug_name        = S("window_hdr_target"),
			.dimension         = RhiTextureDimension_2d,
			.width             = r1->window.width,
			.height            = r1->window.height,
			.multisample_count = r1->multisample_count,
			.usage             = RhiTextureUsage_render_target,
			.format            = PixelFormat_r16g16b16a16_float,
		});
	}
}

fn_local void r1_render_sun_shadows(rhi_command_list_t *list, map_t *map);
fn_local void r1_render_map        (rhi_command_list_t *list, rhi_texture_t rt, map_t *map);
fn_local void r1_post_process      (rhi_command_list_t *list, rhi_texture_t color_hdr, rhi_texture_t rt_result);
fn_local void r1_render_debug_lines(rhi_command_list_t *list, rhi_texture_t rt);

void r1_render_game_view(rhi_command_list_t *list, rhi_texture_t backbuffer, r_view_t *view, map_t *map)
{
	r1->next_region_index = 0;

	R1_TIMED_REGION(list, S("Game View"))
	{
		m4x4_t world_to_clip = mul(view->proj_matrix, view->view_matrix);

		v3_t sun_direction = view->scene.sun_direction;

		m4x4_t sun_view   = make_view_matrix(add(view->camera_p, mul(-256.0f, sun_direction)), 
											 negate(sun_direction), make_v3(0, 0, 1));
		m4x4_t sun_ortho  = make_orthographic_matrix(2048, 2048, 512);
		m4x4_t sun_matrix = mul(sun_ortho, sun_view);

		const rhi_texture_desc_t *backbuffer_desc = rhi_get_texture_desc(backbuffer);

		{
			view_parameters_t view_parameters = {
				.world_to_clip = world_to_clip,
				.sun_matrix    = sun_matrix,
				.sun_direction = sun_direction,
				.sun_color     = view->scene.sun_color,
				.view_size     = make_v2((float)backbuffer_desc->width, (float)backbuffer_desc->height),
			};

			rhi_set_parameters(list, R1ParameterSlot_view, &view_parameters, sizeof(view_parameters));
		}

		rhi_texture_t rt_hdr = r1->window.rt_hdr;

		if (map)
		{
			r1_render_sun_shadows(list, map);
			r1_render_map        (list, rt_hdr, map);

			if (r1->debug_drawing_enabled)
			{
				r1_render_debug_lines(list, rt_hdr);
			}
		}

		r1_post_process(list, rt_hdr, backbuffer);
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

void r1_render_map(rhi_command_list_t *list, rhi_texture_t rt, map_t *map)
{
	R1_TIMED_REGION(list, S("Map"))
	{
		map_pass_parameters_t pass_parameters = {
			.positions     = rhi_get_buffer_srv (r1->map.positions),
			.uvs           = rhi_get_buffer_srv (r1->map.uvs),
			.lightmap_uvs  = rhi_get_buffer_srv (r1->map.lightmap_uvs),
			.sun_shadowmap = rhi_get_texture_srv(r1->shadow_map),
			.shadowmap_dim = { (float)r1->shadow_map_resolution, (float)r1->shadow_map_resolution },
		};
		rhi_set_parameters(list, R1ParameterSlot_pass, &pass_parameters, sizeof(pass_parameters));

		const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(rt);

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
			.viewport = {
				.width  = (float)rt_desc->width,
				.height = (float)rt_desc->height,
				.min_depth = 0.0f,
				.max_depth = 1.0f,
			},
			.scissor_rect = r1->unbounded_scissor_rect,
		});

		{
			rhi_set_pso(list, r1->psos.map);

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

				map_draw_parameters_t draw_parameters = {
					.albedo        = albedo_srv,
					.albedo_dim    = { (float)albedo->w, (float)albedo->h },
					.lightmap      = lightmap_srv,
					.lightmap_dim  = lightmap_dim,
					.normal        = poly->normal,
				};
				rhi_set_parameters(list, R1ParameterSlot_draw, &draw_parameters, sizeof(draw_parameters));

				rhi_draw_indexed(list, r1->map.indices, poly->index_count, poly->first_index, 0);
			}
		}

		rhi_graphics_pass_end(list);
	}
}

void r1_render_debug_lines(rhi_command_list_t *list, rhi_texture_t rt)
{
	if (debug_line_count > 0)
	{
		size_t upload_size = sizeof(debug_line_t)*debug_line_count;
		rhi_upload_buffer_data(r1->debug_lines, 0, debug_lines, upload_size, RhiUploadFreq_frame);

		R1_TIMED_REGION(list, S("Draw Debug Lines"))
		{
			const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(rt);

			rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
				.render_targets[0].texture = rt,
				.depth_stencil.texture     = r1->window.depth_stencil,
				.topology = RhiPrimitiveTopology_linelist,
				.viewport = {
					.width  = (float)rt_desc->width,
					.height = (float)rt_desc->height,
					.min_depth = 0.0f,
					.max_depth = 1.0f,
				},
				.scissor_rect = r1->unbounded_scissor_rect,
			});

			rhi_set_pso(list, r1->psos.debug_lines);

			debug_line_draw_parameters_t draw_parameters = {
				.lines = rhi_get_buffer_srv(r1->debug_lines),
			};
			set_draw_parameters(list, &draw_parameters);

			rhi_draw(list, 2*debug_line_count, 0);

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

			post_process_draw_parameters_t draw_parameters = {
				.hdr_color    = rhi_get_texture_srv(hdr_color),
				.sample_count = r1->multisample_count,
			};
			rhi_set_parameters(list, R1ParameterSlot_draw, &draw_parameters, sizeof(draw_parameters));

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
			rhi_set_parameters(list, R1ParameterSlot_draw, &draw_parameters, sizeof(draw_parameters));

			rhi_draw_instanced(list, 4, 0, (uint32_t)ui_list->count, 0);
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
