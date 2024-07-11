// Generated by metagen.lua

#pragma once

#include "view.h"
#include "bloom.h"
#include "brush.h"
#include "compute_test.h"
#include "debug_lines.h"
#include "fullscreen_triangle.h"
#include "post.h"
#include "shadow.h"
#include "ui.h"
#include "ui_visualize_heatmap.h"

typedef enum df_shader_ident_t
{
	DfShader_none,

	DfShader_bloom_blur_ps, // src/shaders/bloom.metashader

	DfShader_brush_ps, // src/shaders/brush.metashader
	DfShader_brush_vs, // src/shaders/brush.metashader

	DfShader_compute_test_cs, // src/shaders/compute_test.metashader

	DfShader_debug_lines_ps, // src/shaders/debug_lines.metashader
	DfShader_debug_lines_vs, // src/shaders/debug_lines.metashader

	DfShader_fullscreen_triangle_vs, // src/shaders/fullscreen_triangle.metashader

	DfShader_post_ps, // src/shaders/post.metashader
	DfShader_resolve_msaa_ps, // src/shaders/post.metashader

	DfShader_shadow_vs, // src/shaders/shadow.metashader

	DfShader_ui_heatmap_ps, // src/shaders/ui.metashader
	DfShader_ui_ps, // src/shaders/ui.metashader
	DfShader_ui_vs, // src/shaders/ui.metashader

	DfShader_ui_visualize_heatmap_ps, // src/shaders/ui_visualize_heatmap.metashader

	DfShader_COUNT,
} df_shader_ident_t;

global df_shader_info_t df_shaders[DfShader_COUNT];
