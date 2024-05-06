rhi_texture_t color;
rhi_texture_t depth;

rhi_buffer_t index_buffer;
rhi_buffer_t position_buffer;
rhi_buffer_t texcoord_buffer;
rhi_buffer_t lightmap_texcoord_buffer;

rhi_shader_t vs;
rhi_shader_t ps;

rhi_pso_t pso;

void init_map_rendering_resources(map_t *map)
{
	color = rhi_create_texture(1920, 1080, RhiPixelFormat_r8g8b8a8, RhiTextureUsage_render_target);
	depth = rhi_create_texture(1920, 1080, RhiPixelFormat_d24, RhiTextureUsage_depth_stencil);

	size_t index_count       = map->index_count;
	size_t index_buffer_size = sizeof(uint16_t)*index_count;

	size_t vertex_count                  = map->vertex_count;
	size_t position_buffer_size          = sizeof(map->vertex.positions[0])*vertex_count;
	size_t texcoord_buffer_size          = sizeof(map->vertex.texcoords[0])*vertex_count;
	size_t lightmap_texcoord_buffer_size = sizeof(map->vertex.lightmap_texcoords[0])*vertex_count;

	index_buffer             = rhi_create_buffer(index_buffer_size, map->indices);
	position_buffer          = rhi_create_buffer(position_buffer_size, map->vertex.positions);
	texcoord_buffer          = rhi_create_buffer(texcoord_buffer_size, map->vertex.texcoords);
	lightmap_texcoord_buffer = rhi_create_buffer(lightmap_texcoord_buffer_size, map->vertex.lightmap_texcoords);

	vs = asset_get_shader_from_string("map_vs.hlsl");
	ps = asset_get_shader_from_string("map_ps.hlsl");

	pso = rhi_create_graphics_pso(&(rhi_create_graphics_pso_t){
		.vs = vs,
		.ps = ps,
		.depth_stencil = {
			.depth_enable = true,
			.depth_func   = RhiDepthFunc_less_equal,
		},
		.rasterizer = {
			.cull_mode         = RhiCullMode_back,
			.depth_clip_enable = true,
		},
		.topology = RhiTopology_triangle,
	});
}

rhi_viewport_t viewport;
rect2i_t       scissor_rect;

void render_map(rhi_command_list_t *list)
{
	r_draw_stream_t *stream = rhi_begin_render_pass(list, &(rhi_render_pass_t){
		.viewport          = viewport,
		.scissor_rect      = scissor_rect,
		.render_targets[0] = {
			.texture     = color,
			.load_op     = RhiTextureLoadOp_clear,
			.clear_color = make_v4(0.05f, 0.05f, 0.05f, 1.0f),
		},
		.depth_stencil = depth,
		.topology rhi_priRhiPrimitiveTopologyType_     = RhiPrimitiveTopology_triangle_list,
	});

	r_stream_set_pso(stream, pso);

	r_brush_pass_parameters_t *pass_parameters = r_allocate_parameters(r_brush_pass_parameters_t);
	pass_parameters->position_buffer          = position_buffer;
	pass_parameters->texcoord_buffer          = texcoord_buffer;
	pass_parameters->lightmap_texcoord_buffer = lightmap_texcoord_buffer;

	r_stream_set_pass_parameters(stream, parameters);

	for (size_t poly_index = 0; poly_index < map->poly_count; poly_index++)
	{
		map_poly_t *poly = &map->polys[poly_index];

		r_brush_draw_parameters_t *parameters = r_allocate_parameters(r_brush_draw_parameters_t);
		parameters->view     = renderer.view_constants; // doing this in the loop is BS
		parameters->texture  = poly->texture;
		parameters->lightmap = poly->lightmap;

		r_stream_set_draw_parameters(stream, parameters);
		r_stream_draw_indexed(stream, index_buffer, poly->index_count, poly->first_index, poly->first_vertex);
	}

	rhi_end_render_pass(list);
}
