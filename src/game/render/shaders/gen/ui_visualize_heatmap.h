// Generated by metagen.lua

#pragma once

typedef struct ui_visualize_heatmap_draw_parameters_t
{
	rhi_texture_srv_t heatmap;
	float scale;
} ui_visualize_heatmap_draw_parameters_t;

fn void shader_ui_visualize_heatmap_set_draw_params(rhi_command_list_t *list, ui_visualize_heatmap_draw_parameters_t *params);

global string_t ui_visualize_heatmap_source_code;
