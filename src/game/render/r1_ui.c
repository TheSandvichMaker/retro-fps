r1_ui_rect_t make_r1_rect(rect2_t rect)
{
	r1_ui_rect_t result = {
		.origin = mul(0.5f, add(rect.min, rect.max)),
		.radius = mul(0.5f, sub(rect.max, rect.min)), // maybe abs it
	};
	return result;
}

uint16_t r1_push_ui_clip_rect(r1_ui_command_list_t *list, rect2_t rect, v4_t roundedness)
{
	uint16_t result = 0;

	if (ALWAYS(list->clip_rect_count < list->clip_rect_capacity))
	{
		result = list->clip_rect_count++;

		r1_ui_clip_rect_t *clip_rect = &list->clip_rects[result];
		clip_rect->rect        = make_r1_rect(rect);
		clip_rect->roundedness = roundedness;
	}

	return result;
}

r1_ui_command_t *r1_push_ui_command(r1_ui_command_list_t *list, size_t size, r1_ui_command_kind_t kind, uint32_t sort_key)
{
	size_t size_left = list->keys_at - list->commands_at;

	size_t total_size_required = sizeof(r1_ui_command_t) + sizeof(uint32_t); // size for the sort key as well

	if (ALWAYS(size_left >= total_size_required))
	{
		list->keys_at -= sizeof(uint32_t);
		*(uint32_t *)list->keys_at = sort_key;

		r1_ui_command_t *command = (r1_ui_command_t *)list->commands_at;
		list->commands_at += size;

		command->kind = kind;
	}

	return command;
}

void r1_push_ui_box(r1_ui_command_list_t *list, uint8_t group, uint16_t clip_rect, rect2_t rect, v4_t roundedness, 
					r1_ui_colors_t colors, float shadow_radius, float shadow_amount, float inner_radius)
{
	r1_ui_command_t commandheader = r1_push_ui_command(list, R1UiCommand_box);
	command->primitive_group = group;
	command->clip_rect       = clip_rect;

	r1_ui_command_box_t *box = &command->box;
	box->rect          = make_r1_rect(rect);
	box->roundedness   = roundedness;
	box->colors        = colors;
	box->shadow_radius = shadow_radius;
	box->shadow_amount = shadow_amount;
	box->inner_radius  = inner_radius;
}

void r1_push_ui_image(r1_ui_command_list_t *list, uint8_t group, uint16_t clip_rect, rect2_t rect, rect2_t uvs, 
					  rhi_texture_srv_t texture)
{
	r1_ui_command_t *command = r1_push_ui_command(list, R1UiCommand_image);
	command->primitive_group = group;
	command->clip_rect       = clip_rect;

	r1_ui_command_image_t *image = &command->image;
	image->rect    = make_r1_rect(rect);
	image->uvs     = make_r1_rect(uvs);
	image->texture = texture;
}

void r1_create_ui_render_state(r1_ui_render_state_t *state)
{
	m_scoped_temp
	{
		string_t source = fs_read_entire_file(temp, S("../src/shaders/ui_new.hlsl"));

		rhi_shader_bytecode_t vs = rhi_compile_shader(temp, source, S("ui_new.hlsl"), S("MainVS"), S("vs_6_8"));
		rhi_shader_bytecode_t ps = rhi_compile_shader(temp, source, S("ui_new.hlsl"), S("MainPS"), S("ps_6_8"));

		state->pso = rhi_create_graphics_pso(&(rhi_create_graphics_pso_params_t){
			.debug_name = S("pso_ui_new"),
			.vs = vs,
			.ps = ps,
			.blend = {
				.render_target[0] = {
					.blend_enable    = true,
					.src_blend       = RhiBlend_src_alpha,
					.dst_blend       = RhiBlend_inv_src_alpha,
					.blend_op        = RhiBlendOp_add,
					.src_blend_alpha = RhiBlend_inv_dest_alpha,
					.dst_blend_alpha = RhiBlend_one,
					.blend_op_alpha  = RhiBlendOp_add,
					.write_mask      = RhiColorWriteEnable_all,
				},
				.sample_mask = 0xFFFFFFFF,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count     = 1,
			.rtv_formats[0]          = PixelFormat_r8g8b8a8_unorm_srgb,
		});
	}

	state->command_buffer = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("ui_command_buffer"),
		.size       = R1UiCommandBufferSize,
		.flags      = RhiResourceFlag_dynamic,
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = R1UiCommandBufferSize,
			.element_stride = 0,
			.raw            = true,
		},
	});

	state->clip_rect_buffer = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = S("ui_clip_rect_buffer"),
		.size       = sizeof(r1_ui_clip_rect_t)*R1UiClipRectCapacity,
		.flags      = RhiResourceFlag_dynamic,
		.srv = &(rhi_create_buffer_srv_params_t){
			.first_element  = 0,
			.element_count  = R1UiClipRectCapacity,
			.element_stride = sizeof(r1_ui_clip_rect_t),
		},
	});
}

typedef struct r1_ui_draw_params_t
{
	uint32_t         command_count;
	rhi_buffer_srv_t command_buffer;
	rhi_buffer_srv_t clip_rect_buffer;
} r1_ui_draw_params_t;

void r1_render_ui_new(r1_ui_render_state_t *state, rhi_command_list_t *rhi_list, r1_ui_command_list_t *ui_list, rhi_texture_t rt)
{
	//
	// Sort the Sort Keys
	//

	// TODO: This is a perfect candidate for something that can be kicked off earlier in the frame, so that I'm not doing it
	// serially now.
	uint32_t *sort_keys = ui_list->keys_at;
	radix_sort_u32(sort_keys, ui_list->command_count);

	//
	// Upload commands
	//

	size_t command_upload_size = sizeof(r1_ui_command_t)*ui_list->command_count;

	if (command_upload_size > 0)
	{
		r1_ui_command_t *dst_commands = rhi_begin_buffer_upload(state->command_buffer, 0, command_upload_size, RhiUploadFreq_frame);
		{
			for (size_t key_index = 0; key_index < ui_list->command_count; key_index++)
			{
				uint32_t key   = sort_keys[key_index];
				uint32_t index = key & 0xFFFF;

				const r1_ui_command_t *src_command = &ui_list->commands_base[index];
				memcpy(&dst_commands[index], src_command, sizeof(*src_command));
			}
		}
		rhi_end_buffer_upload(state->command_buffer);
	}

	//
	// Draw
	//

	R1_TIMED_REGION(rhi_list, S("Draw UI (new)"))
	{
		rhi_simple_graphics_pass_begin(rhi_list, rt, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
		{
			rhi_set_pso(rhi_list, state->pso);

			r1_ui_draw_params_t params = {
				.command_count    = ui_list->command_count,
				.command_buffer   = r1_get_buffer_srv(state->command_buffer),
				.clip_rect_buffer = r1_get_buffer_srv(state->clip_rect_buffer),
			};
			rhi_set_parameters(rhi_list, R1ParameterSlot_draw, &params, sizeof(params));

			rhi_draw(rhi_list, 3, 0);
		}
		rhi_graphics_pass_end(list);
	}
}
