typedef enum r_parameter_slot_t
{
	R1ParameterSlot_draw = 0,
	R1ParameterSlot_pass = 1,
	R1ParameterSlot_view = 2,
} r_parameter_slot;

typedef struct view_parameters_t
{
	m4x4_t world_to_clip;
	v3_t   sun_direction;
} view_parameters_t;

typedef struct map_pass_parameters_t
{
	alignas(16) rhi_buffer_srv_t positions;
	alignas(16) rhi_buffer_srv_t uvs;
	alignas(16) rhi_buffer_srv_t lightmap_uvs;
} map_pass_parameters_t;

typedef struct map_draw_parameters_t
{
	v3_t     normal;
	uint32_t vertex_offset;
} map_draw_parameters_t;

typedef struct r1_state_t
{
	rhi_texture_t shadow_map;

	struct
	{
		rhi_pso_t map;
	} psos;

	struct
	{
		uint32_t width;
		uint32_t height;
		rhi_texture_t depth_stencil;
	} window;

	struct
	{
		rhi_buffer_t positions;
		rhi_buffer_t uvs;
		rhi_buffer_t lightmap_uvs;
		rhi_buffer_t indices;
	} map;
} r1_state_t;

typedef struct r_view_textures_t
{
	rhi_texture_t hdr_color;
	rhi_texture_t sdr_color;
} r_view_textures_t;

// TODO: view render targets, depth buffer, pso creation
// transient resources

void r1_init(r1_state_t *r1)
{
	r1->shadow_map = rhi_create_texture(&(rhi_create_texture_params_t){
		.dimension = RhiTextureDimension_2d,
		.width     = 1024,
		.height    = 1024,
		.usage     = RhiTextureUsage_depth_stencil|RhiTextureUsage_deny_srv, // TODO: Fix the SRV situation with depth textures
		.format    = RhiPixelFormat_d24_unorm_s8_uint,
	});

	m_scoped_temp
	{
		string_t source = fs_read_entire_file(temp, S("../src/shaders/bindless_draft.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp, source, S("bindless_map.hlsl"), S("MainVS"), S("vs_6_6"));
		rhi_shader_bytecode_t ps = rhi_compile_shader(temp, source, S("bindless_map.hlsl"), S("MainPS"), S("ps_6_6"));

		r1->psos.map = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
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
			.depth_stencil = {
				.depth_test_enable = true,
				.depth_write       = true,
				.depth_func        = RhiComparisonFunc_greater,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = RhiPixelFormat_r8g8b8a8_unorm,
			.dsv_format              = RhiPixelFormat_d24_unorm_s8_uint,
		});
	}
}

void r1_init_map_resources(r1_state_t *r1, map_t *map)
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

fn_local void r1_update_window_resources(r1_state_t *r1, rhi_window_t window)
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
			.debug_name = S("window_depth_buffer"),
			.dimension  = RhiTextureDimension_2d,
			.width      = r1->window.width,
			.height     = r1->window.height,
			.depth      = 1,
			.mip_levels = 1,
			.usage      = RhiTextureUsage_depth_stencil|RhiTextureUsage_deny_srv, // TODO: Deal with depth buffer SRVs
			.format     = RhiPixelFormat_d24_unorm_s8_uint,
		});
	}
}

void r1_render_map(r1_state_t *r1, rhi_command_list_t *list, rhi_window_t window, r_view_t *view, map_t *map)
{
	r1_update_window_resources(r1, window);


	m4x4_t world_to_clip = mul(view->proj_matrix, view->view_matrix);

	view_parameters_t view_parameters = {
		.world_to_clip = world_to_clip,
		.sun_direction = view->scene.sun_direction,
	};

	rhi_set_parameters(list, R1ParameterSlot_view, &view_parameters, sizeof(view_parameters));

	map_pass_parameters_t pass_parameters = {
		.positions    = rhi_get_buffer_srv(r1->map.positions),
		.uvs          = rhi_get_buffer_srv(r1->map.uvs),
		.lightmap_uvs = rhi_get_buffer_srv(r1->map.lightmap_uvs),
	};

	rhi_set_parameters(list, R1ParameterSlot_pass, &pass_parameters, sizeof(pass_parameters));

	rhi_texture_t rt = rhi_get_current_backbuffer(window);

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
		.scissor_rect = {
			.max = { (int32_t)rt_desc->width, (int32_t)rt_desc->height },
		},
	});

	{
		rhi_set_pso(list, r1->psos.map);

		for (size_t i = 0; i < map->poly_count; i++)
		{
			map_poly_t *poly = &map->polys[i];

			map_draw_parameters_t draw_parameters = {
				.normal        = poly->normal,
				.vertex_offset = poly->first_vertex,
			};

			rhi_set_parameters(list, R1ParameterSlot_draw, &draw_parameters, sizeof(draw_parameters));

			rhi_draw_indexed(list, r1->map.indices, poly->index_count, poly->first_index);
		}
	}

	rhi_graphics_pass_end(list);
}

#if 0
void render_game_world(renderer_t *r, r1_view_t *view, world_t *world)
{
	rhi_command_list_t *list = rhi_get_command_list();

	m4x4_t sun_matrix = make_directional_light_matrix(...);

	view_parameters_t view_parameters = {
		.camera_p      = view->camera_p,
		.view_matrix   = view->view_matrix,
		.proj_matrix   = view->proj_matrix,
		.world_to_clip = mul(view->proj_matrix, view->view_matrix),

		.sun_direction = view->sun_direction,
		.sun_color     = view->sun_color,
		.sun_matrix    = sun_matrix,

		.fogmap         = rhi_get_texture_srv(view->fogmap),
		.fog_offset     = view->fog_offset,
		.fog_dim        = view->fog_dim,
		.fog_density    = view->fog_density,
		.fog_absorption = view->fog_absorption,
		.fog_scattering = view->fog_scattering,
		.fog_phase_k    = view->fog_phase_k,
	};

	rhi_set_parameters(list, RenderParameterSlot_view, &view_parameters, sizeof(view_parameters));

	render_skybox(r, list, view);

	map_t *map = world->map;

	if (map)
	{
		render_map_shadows(r, list, view, map);
		render_map        (r, list, view, map);
	}

	render_fog    (r, list, view);
	render_post_fx(r, list, view);
}

void render_skybox(renderer_t *r, rhi_command_list_t *list, r1_view_t *view)
{
	rhi_texture_t render_target = view->hdr_color_target;

	const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(render_target);

	rhi_simple_graphics_pass_begin(list, render_target, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
	{
		rhi_set_pso(list, r->psos.skybox);

		skybox_parameters_t parameters = {
			.positions = rhi_get_buffer_srv (r->skybox_positions),
			.uvs       = rhi_get_buffer_srv (r->skybox_uvs),
			.skybox    = rhi_get_texture_srv(view->skybox),
		};
	}
	rhi_graphics_pass_end(list);
}

void render_map_shadows(renderer_t *r, rhi_command_list_t *list, r1_view_t *view, map_t *map)
{
	rhi_simple_graphics_pass_begin(list, (rhi_texture_t){0}, view->shadowmap, RhiPrimitiveTopology_trianglelist);
	{
		rhi_set_pso(list, r->psos.directional_shadows);

		map_pass_parameters_t pass_parameters = {
			.positions    = map->rhi.positions_srv,
			.uvs          = map->rhi.uvs_srv,
			.lightmap_uvs = map->rhi.lightmap_uvs_srv,
		};

		rhi_set_parameters(list, RenderParameterSlot_pass, &pass_parameters, sizeof(pass_parameters));

		for (size_t i = 0; i < map->poly_count; i++)
		{
			map_poly_t *poly = &map->polys[i];

			map_draw_parameters_t draw_parameters = {
				.albedo        = poly->albedo_rhi,
				.lightmap      = poly->lightmap_rhi,
				.vertex_offset = poly->first_vertex,
			};

			rhi_set_parameters(list, RenderParameterSlot_draw, &draw_parameters[i], sizeof(draw_parameters));

			rhi_draw_indexed(list, map->index_buffer_rhi, poly->index_count, poly->first_index, 0);
		}
	}
	rhi_graphics_pass_end(list);
}

void render_map(renderer_t *r, rhi_command_list_t *list, r1_view_t *view, map_t *map)
{
	rhi_simple_graphics_pass_begin(list, view->hdr_color_target, view->depth_buffer, RhiPrimitiveTopology_trianglelist);
	{
		rhi_set_pso(list, r->map_pso);

		map_pass_parameters_t pass_parameters = {
			.positions    = map->rhi.positions_srv,
			.uvs          = map->rhi.uvs_srv,
			.lightmap_uvs = map->rhi.lightmap_uvs_srv,
			.shadowmap    = rhi_get_texture_srv(view->shadow_map),
		};

		rhi_set_parameters(list, RenderParameterSlot_pass, &pass_parameters, sizeof(pass_parameters));

		for (size_t i = 0; i < map->poly_count; i++)
		{
			map_poly_t *poly = &map->polys[i];

			map_draw_parameters_t draw_parameters = {
				.albedo        = poly->rhi.albedo_srv,
				.lightmap      = poly->rhi.lightmap_srv,
				.vertex_offset = poly->first_vertex,
			};

			rhi_set_parameters(list, RenderParameterSlot_draw, &draw_parameters[i], sizeof(draw_parameters));

			rhi_draw_indexed(list, map->index_buffer_rhi, poly->index_count, poly->first_index, 0);
		}
	}
	rhi_graphics_pass_end(list);
}

void render_fog(renderer_t *r, rhi_command_list_t *list, r1_view_t *view)
{
	rhi_simple_full_screen_pass_begin(list, view->hdr_color_target);
	{
		render_fog_parameters_t parameters = {
			.scene_depth = view->depth_buffer,
			.blue_noise  = rhi_get_texture_srv(r->blue_noise_textures[r->frame_index % ARRAY_COUNT(r->blue_noise_textures)].srv),
			.shadow_map  = rhi_get_texture_srv(r->shadow_map),
		};

		rhi_set_pso(list, r->psos.fog);
		rhi_draw_fullscreen_quad(list);
	}
	rhi_graphics_pass_end(list);
}

void render_post_fx(renderer_t *r, rhi_command_list_t *list, r1_view_t *view)
{
	rhi_simple_full_screen_pass_begin(list, view->sdr_color_target);
	{
		rhi_set_pso(list, r->psos.resolve_hdr);
		rhi_draw_fullscreen_quad(list);
	}
	rhi_graphics_pass_end(list);
}
#endif
