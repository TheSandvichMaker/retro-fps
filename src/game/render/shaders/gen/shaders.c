// Generated by metagen.lua

#include "brush.c"

df_shader_info_t df_shaders[DfShader_COUNT] = {
	[DfShader_brush_ps] = {
		.name        = Sc("brush_ps"),
		.entry_point = Sc("MainPS"),
		.target      = Sc("ps_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/brush.hlsl"),
		.path_dfs    = Sc("src/shaders/brush.dfs"),
	},
	[DfShader_brush_vs] = {
		.name        = Sc("brush_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/brush.hlsl"),
		.path_dfs    = Sc("src/shaders/brush.dfs"),
	},
};
