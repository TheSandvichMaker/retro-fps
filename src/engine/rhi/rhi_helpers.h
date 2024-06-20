#pragma once

//------------------------------------------------------------------------
// Pass helpers

fn_local void rhi_simple_graphics_pass_begin(rhi_command_list_t *list, 
											 rhi_texture_t render_target,
											 rhi_texture_t depth_stencil,
											 rhi_primitive_topology_t topology)
{
	const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(render_target);

	rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
		.render_targets[0] = { .texture = render_target, },
		.depth_stencil     = depth_stencil,
		.topology          = topology,
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
}

fn_local rhi_render_target_blend_t rhi_blend_premul_alpha(void)
{
	rhi_render_target_blend_t result = {
		.blend_enable    = true,
		.src_blend       = RhiBlend_src_alpha,
		.dst_blend       = RhiBlend_inv_src_alpha,
		.blend_op        = RhiBlendOp_add,
		.src_blend_alpha = RhiBlend_inv_dest_alpha,
		.dst_blend_alpha = RhiBlend_one,
		.blend_op_alpha  = RhiBlendOp_add,
		.write_mask      = RhiColorWriteEnable_all,
	};
	return result;
}

fn_local rhi_render_target_blend_t rhi_blend_additive(void)
{
	rhi_render_target_blend_t result = {
		.blend_enable    = true,
		.src_blend       = RhiBlend_one,
		.dst_blend       = RhiBlend_one,
		.blend_op        = RhiBlendOp_add,
		.src_blend_alpha = RhiBlend_one,
		.dst_blend_alpha = RhiBlend_one,
		.blend_op_alpha  = RhiBlendOp_add,
		.write_mask      = RhiColorWriteEnable_all,
	};
	return result;
}

//------------------------------------------------------------------------
// Buffer helpers

fn_local rhi_buffer_t rhi_create_structured_buffer(
	string_t           debug_name, 
	uint32_t           first_element, 
	uint32_t           element_count, 
	uint32_t           element_stride, 
	rhi_buffer_usage_t usage)
{
	rhi_buffer_t result = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = debug_name,
		.desc = {
			.first_element  = first_element,
			.element_count  = element_count,
			.element_stride = element_stride,
		},
		.usage = usage,
	});

	return result;
}

//------------------------------------------------------------------------
// Texture helpers

fn_local void rhi_recreate_texture(rhi_texture_t *texture, const rhi_create_texture_params_t *params)
{
	if (RESOURCE_HANDLE_VALID(*texture))
	{
		rhi_destroy_texture(*texture);
	}

	*texture = rhi_create_texture(params);
}