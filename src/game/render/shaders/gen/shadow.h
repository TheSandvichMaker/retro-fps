// Generated by metagen.lua

#pragma once

typedef struct shadow_draw_parameters_t
{
	rhi_buffer_srv_t positions;
} shadow_draw_parameters_t;

fn void shader_shadow_set_draw_params(rhi_command_list_t *list, shadow_draw_parameters_t *params);

global string_t shadow_source_code;
