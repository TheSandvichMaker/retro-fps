// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

global render_api_i inst;
const render_api_i *const render = &inst;

void equip_render_api(render_api_i *api)
{
    inst = *api;
}

uint32_t r_vertex_format_size[R_VERTEX_FORMAT_COUNT] = {
    [R_VERTEX_FORMAT_POS]       = sizeof(r_vertex_pos_t),
    [R_VERTEX_FORMAT_IMMEDIATE] = sizeof(r_vertex_immediate_t),
    [R_VERTEX_FORMAT_BRUSH]     = sizeof(r_vertex_brush_t),
};

uint64_t r_encode_command_depth(float depth)
{
    float    depth_normalized = saturate(depth / R_COMMAND_DEPTH_FAR_PLANE);
    uint64_t result           = (F32_AS_U32(depth_normalized) >> 3) & MASK_BITS(20);
    return result;
}

r_rect2_fixed_t rect2_to_fixed(rect2_t rect)
{
	r_rect2_fixed_t result;
	result.min_x = (uint16_t)flt_clamp(roundf(rect.min.x), 0.0f, (float)UINT16_MAX);
	result.min_y = (uint16_t)flt_clamp(roundf(rect.min.y), 0.0f, (float)UINT16_MAX);
	result.max_x = (uint16_t)flt_clamp(roundf(rect.max.x), 0.0f, (float)UINT16_MAX);
	result.max_y = (uint16_t)flt_clamp(roundf(rect.max.y), 0.0f, (float)UINT16_MAX);
	return result;
}

rect2_t rect2_from_fixed(r_rect2_fixed_t rect)
{
	rect2_t result = {
		.min.x = (float)rect.min_x,
		.min.y = (float)rect.min_y,
		.max.x = (float)rect.max_x,
		.max.y = (float)rect.max_y,
	};
	return result;
}

r_rect2_fixed_t rect2_fixed_intersect(r_rect2_fixed_t a, r_rect2_fixed_t b)
{
	r_rect2_fixed_t result;
	result.min_x = MAX(a.min_x, b.min_x);
	result.min_y = MAX(a.min_y, b.min_y);
	result.max_x = MIN(a.max_x, b.max_x);
	result.max_y = MIN(a.max_y, b.max_y);
	return result;
}
