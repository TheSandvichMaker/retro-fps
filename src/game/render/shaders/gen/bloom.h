// Generated by metagen.lua from bloom.metashader

#pragma once

typedef struct bloom_draw_parameters_t
{
	rhi_texture_srv_t tex_color;
	v2_t inv_tex_size;
} bloom_draw_parameters_t;

static_assert(sizeof(bloom_draw_parameters_t) <= sizeof(uint32_t)*60, "Draw parameters (which are passed as root constants) can't be larger than 60 uint32s");

fn void shader_set_params__bloom_draw_parameters_t(rhi_command_list_t *list, uint32_t slot, bloom_draw_parameters_t *parameters);

global string_t bloom_source_code;
