// Generated by metagen.lua from post.metashader

#pragma once

typedef struct post_draw_parameters_t
{
	rhi_texture_srv_t bloom;
	float bloom_amount;
	v2u_t pad0;
	rhi_texture_srv_t blue_noise;
	v3u_t pad1;
	rhi_texture_srv_t resolved_color;
} post_draw_parameters_t;

static_assert(sizeof(post_draw_parameters_t) <= sizeof(uint32_t)*60, "Draw parameters (which are passed as root constants) can't be larger than 60 uint32s");

fn void shader_set_params__post_draw_parameters_t(rhi_command_list_t *list, uint32_t slot, post_draw_parameters_t *parameters);

global string_t post_source_code;
