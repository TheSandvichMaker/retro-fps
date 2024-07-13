// Generated by metagen.lua

#include "view.c"
#include "bloom.c"
#include "brush.c"
#include "compute_test.c"
#include "debug_lines.c"
#include "fullscreen_triangle.c"
#include "post.c"
#include "shadow.c"
#include "texture_viewer.c"
#include "ui.c"
#include "ui_visualize_heatmap.c"

df_shader_info_t df_shaders[DfShader_COUNT] = {
	[DfShader_bloom_blur_ps] = {
		.name        = Sc("bloom_blur_ps"),
		.entry_point = Sc("bloom_blur_ps"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_BLOOM_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/bloom.hlsl"),
		.path_dfs    = Sc("src/shaders/bloom.metashader"),
	},

	[DfShader_brush_ps] = {
		.name        = Sc("brush_ps"),
		.entry_point = Sc("brush_ps"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_BRUSH_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/brush.hlsl"),
		.path_dfs    = Sc("src/shaders/brush.metashader"),
	},
	[DfShader_brush_vs] = {
		.name        = Sc("brush_vs"),
		.entry_point = Sc("brush_vs"),
		.target      = Sc("vs_6_6"),
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
	[DfShader_resolve_msaa_ps] = {
		.name        = Sc("resolve_msaa_ps"),
		.entry_point = Sc("resolve_msaa_ps"),
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

	[DfShader_texture_viewer_ps] = {
		.name        = Sc("texture_viewer_ps"),
		.entry_point = Sc("texture_viewer_ps"),
		.target      = Sc("ps_6_6"),
		.hlsl_source = Sc(DF_SHADER_TEXTURE_VIEWER_SOURCE_CODE),
		.path_hlsl   = Sc("src/shaders/gen/texture_viewer.hlsl"),
		.path_dfs    = Sc("src/shaders/texture_viewer.metashader"),
	},

	[DfShader_ui_heatmap_ps] = {
		.name        = Sc("ui_heatmap_ps"),
		.entry_point = Sc("UIHeatMapPS"),
		.target      = Sc("ps_6_6"),
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
	[DfShader_ui_vs] = {
		.name        = Sc("ui_vs"),
		.entry_point = Sc("MainVS"),
		.target      = Sc("vs_6_6"),
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

df_pso_info_t df_psos[DfPso_COUNT] = {
	[DfPso_bloom_blur] = {
		.name = Sc("bloom_blur"),
		.path = Sc("src/shaders/bloom.metashader"),
		.vs_index = DfShader_fullscreen_triangle_vs,
		.ps_index = DfShader_bloom_blur_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_bloom_blur"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0].write_mask = RhiColorWriteEnable_all,
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r16g16b16a16_float,
			},
			.multisample_count = 1,
		},
	},
	[DfPso_brush] = {
		.name = Sc("brush"),
		.path = Sc("src/shaders/brush.metashader"),
		.vs_index = DfShader_brush_vs,
		.ps_index = DfShader_brush_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_brush"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0].write_mask = RhiColorWriteEnable_all,
			.rasterizer.multisample_enable = true,
			.depth_stencil.depth_test_enable = true,
			.depth_stencil.depth_write = true,
			.depth_stencil.depth_func = RhiComparisonFunc_greater,
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r16g16b16a16_float,
			},
			.dsv_format = PixelFormat_d24_unorm_s8_uint,
			.multisample_count = 1,
		},
	},
	[DfPso_debug_lines] = {
		.name = Sc("debug_lines"),
		.path = Sc("src/shaders/debug_lines.metashader"),
		.vs_index = DfShader_debug_lines_vs,
		.ps_index = DfShader_debug_lines_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_debug_lines"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0].write_mask = RhiColorWriteEnable_all,
			.rasterizer.cull_mode = RhiCullMode_back,
			.rasterizer.multisample_enable = true,
			.depth_stencil.depth_test_enable = true,
			.depth_stencil.depth_func = RhiComparisonFunc_greater,
			.primitive_topology_type = RhiPrimitiveTopologyType_line,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r16g16b16a16_float,
			},
			.dsv_format = PixelFormat_d24_unorm_s8_uint,
			.multisample_count = 1,
		},
	},
	[DfPso_post_process] = {
		.name = Sc("post_process"),
		.path = Sc("src/shaders/post.metashader"),
		.vs_index = DfShader_fullscreen_triangle_vs,
		.ps_index = DfShader_post_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_post_process"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0].write_mask = RhiColorWriteEnable_all,
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r8g8b8a8_unorm_srgb,
			},
			.multisample_count = 1,
		},
	},
	[DfPso_resolve_msaa] = {
		.name = Sc("resolve_msaa"),
		.path = Sc("src/shaders/post.metashader"),
		.vs_index = DfShader_fullscreen_triangle_vs,
		.ps_index = DfShader_resolve_msaa_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_resolve_msaa"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0].write_mask = RhiColorWriteEnable_all,
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r16g16b16a16_float,
			},
			.multisample_count = 1,
		},
	},
	[DfPso_sun_shadows] = {
		.name = Sc("sun_shadows"),
		.path = Sc("src/shaders/shadow.metashader"),
		.vs_index = DfShader_shadow_vs,
		.params_without_bytecode = {
			.debug_name = Sc("pso_sun_shadows"),
			.blend.sample_mask = 0xFFFFFFFF,
			.rasterizer.cull_mode = RhiCullMode_back,
			.depth_stencil.depth_test_enable = true,
			.depth_stencil.depth_write = true,
			.depth_stencil.depth_func = RhiComparisonFunc_greater,
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.dsv_format = PixelFormat_d24_unorm_s8_uint,
			.multisample_count = 1,
		},
	},
	[DfPso_texture_viewer] = {
		.name = Sc("texture_viewer"),
		.path = Sc("src/shaders/texture_viewer.metashader"),
		.vs_index = DfShader_fullscreen_triangle_vs,
		.ps_index = DfShader_texture_viewer_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_texture_viewer"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0] = {
				.blend_enable = true,
				.src_blend = RhiBlend_src_alpha,
				.dst_blend = RhiBlend_inv_src_alpha,
				.blend_op = RhiBlendOp_add,
				.src_blend_alpha = RhiBlend_inv_dest_alpha,
				.dst_blend_alpha = RhiBlend_one,
				.blend_op_alpha = RhiBlendOp_add,
				.write_mask = RhiColorWriteEnable_all,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r8g8b8a8_unorm_srgb,
			},
			.multisample_count = 1,
		},
	},
	[DfPso_ui] = {
		.name = Sc("ui"),
		.path = Sc("src/shaders/ui.metashader"),
		.vs_index = DfShader_ui_vs,
		.ps_index = DfShader_ui_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_ui"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0] = {
				.blend_enable = true,
				.src_blend = RhiBlend_src_alpha,
				.dst_blend = RhiBlend_inv_src_alpha,
				.blend_op = RhiBlendOp_add,
				.src_blend_alpha = RhiBlend_inv_dest_alpha,
				.dst_blend_alpha = RhiBlend_one,
				.blend_op_alpha = RhiBlendOp_add,
				.write_mask = RhiColorWriteEnable_all,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r8g8b8a8_unorm_srgb,
			},
			.multisample_count = 1,
		},
	},
	[DfPso_ui_heatmap] = {
		.name = Sc("ui_heatmap"),
		.path = Sc("src/shaders/ui.metashader"),
		.vs_index = DfShader_ui_vs,
		.ps_index = DfShader_ui_heatmap_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_ui_heatmap"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0] = {
				.blend_enable = true,
				.src_blend = RhiBlend_one,
				.dst_blend = RhiBlend_one,
				.blend_op = RhiBlendOp_add,
				.src_blend_alpha = RhiBlend_one,
				.dst_blend_alpha = RhiBlend_one,
				.blend_op_alpha = RhiBlendOp_add,
				.write_mask = RhiColorWriteEnable_all,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r8g8b8a8_unorm,
			},
			.multisample_count = 1,
		},
	},
	[DfPso_ui_visualize_heatmap] = {
		.name = Sc("ui_visualize_heatmap"),
		.path = Sc("src/shaders/ui_visualize_heatmap.metashader"),
		.vs_index = DfShader_fullscreen_triangle_vs,
		.ps_index = DfShader_ui_visualize_heatmap_ps,
		.params_without_bytecode = {
			.debug_name = Sc("pso_ui_visualize_heatmap"),
			.blend.sample_mask = 0xFFFFFFFF,
			.blend.render_target[0] = {
				.blend_enable = true,
				.src_blend = RhiBlend_src_alpha,
				.dst_blend = RhiBlend_inv_src_alpha,
				.blend_op = RhiBlendOp_add,
				.src_blend_alpha = RhiBlend_inv_dest_alpha,
				.dst_blend_alpha = RhiBlend_one,
				.blend_op_alpha = RhiBlendOp_add,
				.write_mask = RhiColorWriteEnable_all,
			},
			.primitive_topology_type = RhiPrimitiveTopologyType_triangle,
			.render_target_count = 1,
			.rtv_formats = {
				PixelFormat_r8g8b8a8_unorm_srgb,
			},
			.multisample_count = 1,
		},
	},
};

