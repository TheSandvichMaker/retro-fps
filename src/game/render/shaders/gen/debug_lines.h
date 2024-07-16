// Generated by metagen.lua

#pragma once

typedef struct debug_lines_draw_parameters_t
{
	rhi_buffer_srv_t lines;
} debug_lines_draw_parameters_t;

static_assert(sizeof(debug_lines_draw_parameters_t) <= sizeof(uint32_t)*60, "Draw parameters (which are passed as root constants) can't be larger than 60 uint32s");

fn void shader_set_params__debug_lines_draw_parameters_t(rhi_command_list_t *list, uint32_t slot, debug_lines_draw_parameters_t *parameters);global string_t debug_lines_source_code;
