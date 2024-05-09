typedef enum r_parameter_slot_t
{
	RenderParameterSlot_draw = 0,
	RenderParameterSlot_pass = 1,
	RenderParameterSlot_view = 2,
} r_parameter_slot;

typedef struct map_pass_parameters_t
{
	rhi_buffer_srv_t positions;
	rhi_buffer_srv_t uvs;
	rhi_buffer_srv_t lightmap_uvs;
} map_pass_parameters_t;

typedef struct renderer_t
{
	rhi_texture_t shadow_map;

	struct
	{
		rhi_pso_t map;
	} psos;
} renderer_t;

// TODO: view render targets, depth buffer, pso creation

void init_renderer(renderer_t *r)
{
	r->shadow_map = rhi_create_texture(&(rhi_create_texture_t){
		.width  = 1024,
		.height = 1024,
		.usage  = RhiTextureUsage_depth_stencil,
		.format = RhiPixelFormat_d24,
		.per_frame_resource = true,
	});

	m_scoped_temp
	{
		string_t map_shader = fs_read_entire_file(temp, S("../src/shaders/bindless_map.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp,
													  shader_source, 
													  S("bindless_map.hlsl"),
													  S("MainVS"),
													  S("vs_6_6"));

		rhi_shader_bytecode_t ps = rhi_compile_shader(temp,
													  shader_source, 
													  S("bindless_map.hlsl"),
													  S("MainPS"),
													  S("ps_6_6"));

		r->map_pso = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.vs = vs,
			.ps = ps,
			.blend = {
				.render_target[0].write_mask = RhiColorWriteEnable_all,
				.sample_mask                 = 0xFFFFFFFF,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = RhiPixelFormat_r8g8b8a8_unorm,
		});
	}
}

void render_game_world(renderer_t *r, r_view_t *view, world_t *world)
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

void render_skybox(renderer_t *r, rhi_command_list_t *list, r_view_t *view)
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

void render_map_shadows(renderer_t *r, rhi_command_list_t *list, r_view_t *view, map_t *map)
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

void render_map(renderer_t *r, rhi_command_list_t *list, r_view_t *view, map_t *map)
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

void render_fog(renderer_t *r, rhi_command_list_t *list, r_view_t *view)
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

void render_post_fx(renderer_t *r, rhi_command_list_t *list, r_view_t *view)
{
	rhi_simple_full_screen_pass_begin(list, view->sdr_color_target);
	{
		rhi_set_pso(list, r->psos.resolve_hdr);
		rhi_draw_fullscreen_quad(list);
	}
	rhi_graphics_pass_end(list);
}
