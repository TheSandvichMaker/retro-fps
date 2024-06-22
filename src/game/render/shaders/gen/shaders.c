// Generated by metagen.lua

#include "view.c"
#include "brush.c"
#include "compute_test.c"
#include "debug_lines.c"
#include "fullscreen_triangle.c"
#include "post.c"
#include "shadow.c"
#include "ui.c"
#include "ui_visualize_heatmap.c"

df_shader_info_t df_shaders[DfShader_COUNT] = {
	[DfShader_brush_vs] = {
		.name        = Sc("brush_vs"),
		.entry_point = Sc("brush_vs"),
		.target      = Sc("vs_6_6"),
		.hlsl_source = Sc(DF_SHADER_BRUSH_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/brush.hlsl"),
		.path_dfs    = Sc("src/shaders/brush.metashader"),
	},
	[DfShader_brush_ps] = {
		.name        = Sc("brush_ps"),
		.entry_point = Sc("brush_ps"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_BRUSH_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/brush.hlsl"),
		.path_dfs    = Sc("src/shaders/brush.metashader"),
	},

	[DfShader_compute_test_cs] = {
		.name        = Sc("compute_test_cs"),
		.entry_point = Sc("MainCS"),
		.target      = Sc("cs_6_6"),
		.hlsl_source = Sc(DF_SHADER_COMPUTE_TEST_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/compute_test.hlsl"),
		.path_dfs    = Sc("src/shaders/compute_test.metashader"),
	},

	[DfShader_debug_lines_ps] = {
		.name        = Sc("debug_lines_ps"),
		.entry_point = Sc("MainPS"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_DEBUG_LINES_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/debug_lines.hlsl"),
		.path_dfs    = Sc("src/shaders/debug_lines.metashader"),
	},
	[DfShader_debug_lines_vs] = {
		.name        = Sc("debug_lines_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.hlsl_source = Sc(DF_SHADER_DEBUG_LINES_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/debug_lines.hlsl"),
		.path_dfs    = Sc("src/shaders/debug_lines.metashader"),
	},

	[DfShader_fullscreen_triangle_vs] = {
		.name        = Sc("fullscreen_triangle_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.hlsl_source = Sc(DF_SHADER_FULLSCREEN_TRIANGLE_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/fullscreen_triangle.hlsl"),
		.path_dfs    = Sc("src/shaders/fullscreen_triangle.metashader"),
	},

	[DfShader_post_ps] = {
		.name        = Sc("post_ps"),
		.entry_point = Sc("post_ps"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_POST_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/post.hlsl"),
		.path_dfs    = Sc("src/shaders/post.metashader"),
	},

	[DfShader_shadow_vs] = {
		.name        = Sc("shadow_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.hlsl_source = Sc(DF_SHADER_SHADOW_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/shadow.hlsl"),
		.path_dfs    = Sc("src/shaders/shadow.metashader"),
	},

	[DfShader_ui_heatmap_ps] = {
		.name        = Sc("ui_heatmap_ps"),
		.entry_point = Sc("UIHeatMapPS"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_UI_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/ui.hlsl"),
		.path_dfs    = Sc("src/shaders/ui.metashader"),
	},
	[DfShader_ui_vs] = {
		.name        = Sc("ui_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
		.hlsl_source = Sc(DF_SHADER_UI_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/ui.hlsl"),
		.path_dfs    = Sc("src/shaders/ui.metashader"),
	},
	[DfShader_ui_ps] = {
		.name        = Sc("ui_ps"),
		.entry_point = Sc("MainPS"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_UI_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/ui.hlsl"),
		.path_dfs    = Sc("src/shaders/ui.metashader"),
	},

	[DfShader_ui_visualize_heatmap_ps] = {
		.name        = Sc("ui_visualize_heatmap_ps"),
		.entry_point = Sc("MainPS"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_UI_VISUALIZE_HEATMAP_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/ui_visualize_heatmap.hlsl"),
		.path_dfs    = Sc("src/shaders/ui_visualize_heatmap.metashader"),
	},
};
