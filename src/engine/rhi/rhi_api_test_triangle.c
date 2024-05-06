global rhi_buffer_t g_positions;
global rhi_buffer_t g_colors;

global rhi_buffer_srv_t g_positions_srv;
global rhi_buffer_srv_t g_colors_srv;

global rhi_pso_t g_pso;

typedef struct pass_parameters_t
{
	rhi_buffer_srv_t positions;
	rhi_buffer_srv_t colors;
} pass_parameters_t;

typedef struct shader_parameters_t
{
	v4_t offset;
	v4_t color;
} shader_parameters_t;

fn void rhi_api_test_triangle_init(void)
{
	const float aspect = 16.0f / 9.0f;

	v3_t positions[] = {
		{  0.00f,  0.25f * aspect, 0.0f },
		{  0.25f, -0.25f * aspect, 0.0f },
		{ -0.25f, -0.25f * aspect, 0.0f },
	};

	v4_t colors[] = {
		{ 1, 0, 0, 1 },
		{ 0, 1, 0, 1 },
		{ 0, 0, 1, 1 },
	};

	g_positions = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("triangle positions"),
		.size = sizeof(positions),
		.initial_data = {
			.ptr    = positions,
			.offset = 0,
			.size   = sizeof(positions),
		},
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = ARRAY_COUNT(positions),
			.element_stride = sizeof(positions[0]),
		},
	});

	g_positions_srv = rhi_get_buffer_srv(g_positions);

	g_colors = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("triangle colors"),
		.size = sizeof(colors),
		.initial_data = {
			.ptr    = colors,
			.offset = 0,
			.size   = sizeof(colors),
		},
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = ARRAY_COUNT(colors),
			.element_stride = sizeof(colors[0]),
		},
	});

	g_colors_srv = rhi_get_buffer_srv(g_colors);

	m_scoped_temp
	{
		string_t shader_source = fs_read_entire_file(temp, S("../src/shaders/bindless_triangle.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp,
													  shader_source, 
													  S("bindless_triangle.hlsl"),
													  S("MainVS"),
													  S("vs_6_6"));

		rhi_shader_bytecode_t ps = rhi_compile_shader(temp,
													  shader_source, 
													  S("bindless_triangle.hlsl"),
													  S("MainPS"),
													  S("ps_6_6"));

		g_pso = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.vs = vs,
			.ps = ps,
			.blend = {
				.render_target[0] = {
					.write_mask = RhiColorWriteEnable_all,
				},
				.sample_mask = 0xFFFFFFFF,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = RhiPixelFormat_r8g8b8a8_unorm,
		});
	}
}

fn void rhi_api_test_triangle_draw(rhi_window_t window, rhi_command_list_t *list)
{
	rhi_texture_t render_target = rhi_get_current_backbuffer(window);

	const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(render_target);

	rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
		.render_targets[0] = {
			.texture     = render_target,
			.op          = RhiPassOp_clear,
			.clear_color = make_v4(0.15f, 0.25f, 0.15f, 1.0f),
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

	rhi_set_pso(list, g_pso);

	pass_parameters_t pass_parameters = {
		.positions = g_positions_srv,
		.colors    = g_colors_srv,
	};

	rhi_set_parameters(list, 1, &pass_parameters, sizeof(pass_parameters));

	static float animation_time = 0.0f;

	const float animation_t = sin_ss(animation_time);
	animation_time += 1.0f / 60.0f;

	shader_parameters_t triangle_parameters[] = {
		{ { animation_t * -0.1,  0.0 }, { 1, 0, 0, 1 } },
		{ { animation_t *  0.1,  0.2 }, { 0, 1, 0, 1 } },
		{ { animation_t * -0.3, -0.3 }, { 0, 0, 1, 1 } },
	};

	for (size_t i = 0; i < ARRAY_COUNT(triangle_parameters); i++)
	{
		rhi_set_parameters(list, 0, &triangle_parameters[i], sizeof(triangle_parameters));
		rhi_command_list_draw(list, 3);
	}

	rhi_graphics_pass_end(list);
}
