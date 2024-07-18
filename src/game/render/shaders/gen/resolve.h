// Generated by metagen.lua from resolve.metashader

#pragma once

typedef struct resolve_draw_parameters_t
{
	rhi_texture_srv_t blue_noise;
	int32_t sample_count;
	v2u_t pad0;
	rhi_texture_srv_t depth_buffer;
	v3u_t pad1;
	rhi_texture_srv_t hdr_color;
	v3u_t pad2;
	rhi_texture_srv_t shadow_map;
} resolve_draw_parameters_t;

static_assert(sizeof(resolve_draw_parameters_t) <= sizeof(uint32_t)*60, "Draw parameters (which are passed as root constants) can't be larger than 60 uint32s");

fn void shader_set_params__resolve_draw_parameters_t(rhi_command_list_t *list, uint32_t slot, resolve_draw_parameters_t *parameters);

global string_t resolve_source_code;
