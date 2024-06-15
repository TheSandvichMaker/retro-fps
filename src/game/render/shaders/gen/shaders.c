// Generated by metagen.lua

#include "view.c"
#include "brush.c"
#include "debug_lines.c"
#include "post.c"
#include "shadow.c"
#include "ui.c"

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
	[DfShader_debug_lines_ps] = {
		.name        = Sc("debug_lines_ps"),
		.entry_point = Sc("MainPS"),
		.target      = Sc("ps_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/debug_lines.hlsl"),
		.path_dfs    = Sc("src/shaders/debug_lines.dfs"),
	},

	[DfShader_debug_lines_vs] = {
		.name        = Sc("debug_lines_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/debug_lines.hlsl"),
		.path_dfs    = Sc("src/shaders/debug_lines.dfs"),
	},
	[DfShader_post_ps] = {
		.name        = Sc("post_ps"),
		.entry_point = Sc("MainPS"),
		.target      = Sc("ps_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/post.hlsl"),
		.path_dfs    = Sc("src/shaders/post.dfs"),
	},

	[DfShader_post_vs] = {
		.name        = Sc("post_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/post.hlsl"),
		.path_dfs    = Sc("src/shaders/post.dfs"),
	},
	[DfShader_shadow_vs] = {
		.name        = Sc("shadow_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/shadow.hlsl"),
		.path_dfs    = Sc("src/shaders/shadow.dfs"),
	},
	[DfShader_ui_ps] = {
		.name        = Sc("ui_ps"),
		.entry_point = Sc("MainPS"),
		.target      = Sc("ps_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/ui.hlsl"),
		.path_dfs    = Sc("src/shaders/ui.dfs"),
	},

	[DfShader_ui_vs] = {
		.name        = Sc("ui_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.path_hlsl   = Sc("src/shaders/gen/ui.hlsl"),
		.path_dfs    = Sc("src/shaders/ui.dfs"),
	},
};
