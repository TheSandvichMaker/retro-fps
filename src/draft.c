
typedef struct r_vertex_map_t
{
    v3_t pos;
    v2_t tex;
    v2_t tex_lightmap;
    v3_t normal;
	uint32_t texture_index;
};

rhi_texture_t color;
rhi_texture_t depth;

rhi_buffer_t index_buffer;
rhi_buffer_t vertex_buffer;

rhi_shader_t vs;
rhi_shader_t ps;

rhi_pso_t pso;

void init_map_rendering_resources(map_t *map)
{
	color = rhi_create_texture(1920, 1080, RhiPixelFormat_r8g8b8a8, RhiTextureUsage_render_target);
	depth = rhi_create_texture(1920, 1080, RhiPixelFormat_d24, RhiTextureUsage_depth_stencil);

	size_teindex_count        = map->index_count;
	size_t index_buffer_size  = sizeof(uint16_t)*index_count;
	size_t vertex_count       = map->vertex_count;
	size_t vertex_buffer_size = sizeof(r_vertex_map_t)*vertex_count;

	index_buffer  = rhi_create_buffer(index_buffer_size, map->indices);
	vertex_buffer = rhi_create_buffer(vertex_buffer_size, map->vertices);

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
rect2_t        scissor_rect;

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
		.topology      = RhiPrimitiveTopology_triangle_list,
	});

	my_uniforms_t *uniforms = r_allocate_uniforms(my_uniforms_t);
	uniforms->texture = rhi_get_resource_index(texture);
	uniforms->buffer  = rhi_get_resource_index(buffer);

	r_stream_set_pso          (stream, pso);
	r_stream_set_index_buffer (stream, index_buffer);
	r_stream_set_vertex_buffer(stream, 0, vertex_buffer);
	r_stream_set_index_count  (stream, 3);
	r_stream_draw();

	rhi_end_render_pass(list);
}
