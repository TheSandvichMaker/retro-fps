// Generated by metagen.lua

void shader_brush_set_draw_params(rhi_command_list_t *list, brush_draw_parameters_t *params)
{
	rhi_set_parameters(list, 0, params, sizeof(*params));
}

void shader_brush_set_pass_params(rhi_command_list_t *list, brush_pass_parameters_t *params)
{
	rhi_set_parameters(list, 1, params, sizeof(*params));
}

