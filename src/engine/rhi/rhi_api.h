// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#include "rhi_formats.h"

enum { RhiMaxRenderTargetCount = 8 };

typedef struct rhi_init_params_t
{
	uint32_t frame_latency;
} rhi_init_params_t;

typedef enum rhi_backend_t
{
	RhiBackend_none  = 0,
	RhiBackend_d3d12 = 1,
	RhiBackend_COUNT,
} rhi_backend_t;

DEFINE_HANDLE_TYPE(rhi_window_t);
DEFINE_HANDLE_TYPE(rhi_buffer_t);
DEFINE_HANDLE_TYPE(rhi_texture_t);
DEFINE_HANDLE_TYPE(rhi_pso_t);

fn void rhi_resize_window(rhi_window_t window, uint32_t new_width, uint32_t new_height);
fn rhi_texture_t rhi_get_current_backbuffer(rhi_window_t window);
fn v2_t rhi_get_window_client_size(rhi_window_t window);

typedef struct rhi_frame_statistics_t
{
	uint64_t sync_cpu_time;
} rhi_frame_statistics_t;

fn uint64_t rhi_get_frame_sync_cpu_freq(void);
fn bool rhi_get_frame_statistics(rhi_window_t window, rhi_frame_statistics_t *statistics);
fn void rhi_wait_on_swap_chain(rhi_window_t window);
fn bool rhi_get_window_fullscreen(rhi_window_t window);
fn void rhi_set_window_fullscreen(rhi_window_t window, bool fullscreen);

//
// resources
//

typedef struct rhi_texture_srv_t
{
	uint32_t index;
} rhi_texture_srv_t;

typedef struct rhi_buffer_srv_t
{
	uint32_t index;
} rhi_buffer_srv_t;

typedef struct rhi_buffer_uav_t
{
	uint32_t index;
} rhi_buffer_uav_t;

typedef enum rhi_texture_usage_t
{
	RhiTextureUsage_render_target = 0x1,
	RhiTextureUsage_depth_stencil = 0x2,
	RhiTextureUsage_uav           = 0x4,
	RhiTextureUsage_deny_srv      = 0x8, // srv is allowed by default :)
} rhi_texture_usage_t;

typedef enum rhi_texture_dimension_t
{
	RhiTextureDimension_unknown = 0,
	RhiTextureDimension_1d      = 1,
	RhiTextureDimension_2d      = 2,
	RhiTextureDimension_3d      = 3,
} rhi_texture_dimension_t;

typedef struct rhi_texture_desc_t
{
	rhi_texture_dimension_t dimension;
	uint32_t                width;
	uint32_t                height;
	uint32_t                depth;
	uint32_t                mip_levels;
	pixel_format_t          format;
	uint32_t                sample_count;
	rhi_texture_usage_t     usage_flags;
} rhi_texture_desc_t;

fn const rhi_texture_desc_t *rhi_get_texture_desc(rhi_texture_t texture);

typedef struct rhi_texture_data_t
{
	void    **subresources;
	uint32_t  subresource_count, offset;
	uint32_t  row_stride, slice_stride;
} rhi_texture_data_t;

typedef enum rhi_resource_flags_t
{
	RhiResourceFlag_dynamic = 0x1, // that is - can be updated dynamically while rendering. causes the resource to be double/triple buffered
} rhi_resource_flags_t;

typedef struct rhi_create_texture_params_t
{
	string_t                debug_name;
	rhi_texture_dimension_t dimension;

	uint32_t                width;
	uint32_t                height;
	uint32_t                depth;
	uint32_t                mip_levels;
	uint32_t                multisample_count;

	rhi_resource_flags_t    flags;

	rhi_texture_usage_t     usage;
	pixel_format_t          format;

	v4_t                    optimized_clear_value;

	rhi_texture_data_t     *initial_data;
} rhi_create_texture_params_t;

typedef enum rhi_upload_frequency_t
{
	RhiUploadFreq_frame,
	RhiUploadFreq_background,
} rhi_upload_frequency_t;

fn rhi_texture_t     rhi_create_texture         (const rhi_create_texture_params_t *params);
fn void              rhi_destroy_texture        (rhi_texture_t handle);
fn void              rhi_upload_texture_data    (rhi_texture_t texture, const rhi_texture_data_t *data);
fn bool              rhi_texture_upload_complete(rhi_texture_t texture);
fn void              rhi_wait_on_texture_upload (rhi_texture_t texture);
fn rhi_texture_srv_t rhi_get_texture_srv        (rhi_texture_t texture);
fn void              rhi_validate_texture_srv   (rhi_texture_srv_t srv, string_t context); // tries to verify that this srv is valid

typedef enum rhi_heap_kind_t
{
	RhiHeapKind_default,
	// RhiHeapKind_upload, not sure I care to expose this
	RhiHeapKind_readback,

	RhiHeapKind_COUNT,
} rhi_heap_kind_t;

typedef struct rhi_buffer_data_t
{
	void     *src;
	uint64_t  dst_offset;
	uint64_t  size;
} rhi_buffer_data_t;

typedef uint32_t rhi_buffer_flags_t;
typedef enum rhi_buffer_flags_enum_t
{
	RhiBufferFlag_raw = 0x1,
} rhi_buffer_flags_enum_t;

typedef uint32_t rhi_buffer_usage_t;
typedef enum rhi_buffer_usage_enum_t
{
	RhiBufferUsage_uav           = 0x4,
	RhiBufferUsage_deny_srv      = 0x8, // srv is allowed by default :)
} rhi_buffer_usage_enum_t;

typedef struct rhi_buffer_desc_t
{
	uint64_t           first_element;
	uint64_t           element_count;
	uint64_t           element_stride;
	uint64_t           counter_offset_in_bytes;
	rhi_buffer_flags_t flags;
} rhi_buffer_desc_t;

typedef struct rhi_create_buffer_params_t
{
	string_t             debug_name;
	rhi_buffer_desc_t    desc;
	rhi_heap_kind_t      heap;
	rhi_buffer_usage_t   usage;
	rhi_resource_flags_t flags;
	rhi_buffer_data_t    initial_data;
} rhi_create_buffer_params_t;

fn rhi_buffer_t     rhi_create_buffer(const rhi_create_buffer_params_t *params);
fn void             rhi_upload_buffer_data(rhi_buffer_t buffer, size_t dst_offset, const void *src, size_t src_size, rhi_upload_frequency_t frequency);
fn rhi_buffer_srv_t rhi_get_buffer_srv(rhi_buffer_t buffer);
fn rhi_buffer_uav_t rhi_get_buffer_uav(rhi_buffer_t handle);
fn void             rhi_validate_buffer_srv(rhi_buffer_srv_t srv, string_t context); // tries to verify that this srv is valid

fn void *rhi_begin_buffer_upload  (rhi_buffer_t buffer, size_t offset, size_t size, rhi_upload_frequency_t frequency);
fn void  rhi_end_buffer_upload    (rhi_buffer_t buffer);
fn void  rhi_wait_on_buffer_upload(rhi_buffer_t buffer);

fn void *rhi_map  (rhi_buffer_t buffer);
fn void  rhi_unmap(rhi_buffer_t buffer);

fn rhi_buffer_t rhi_create_structured_buffer(
	string_t           debug_name, 
	uint32_t           first_element, 
	uint32_t           element_count, 
	uint32_t           element_stride, 
	rhi_buffer_usage_t usage)
{
	rhi_buffer_t result = rhi_create_buffer(&(rhi_create_buffer_params_t){
		.debug_name = debug_name,
		.desc = {
			.first_element  = first_element,
			.element_count  = element_count,
			.element_stride = element_stride,
		},
		.usage = usage,
	});

	return result;
}

typedef struct rhi_shader_bytecode_t
{
	const uint8_t *bytes;
	size_t         count;
} rhi_shader_bytecode_t;

fn rhi_shader_bytecode_t rhi_compile_shader(arena_t *arena,
											string_t shader_source,  // source code in utf8
											string_t file_name,      // shader file name only used for debug
											string_t entry_point,
											string_t shader_model);

//
// graphics
//

typedef struct rhi_command_list_t rhi_command_list_t;

typedef enum rhi_comparison_func_t 
{
	RhiComparisonFunc_none          = 0,
	RhiComparisonFunc_never         = 1,
	RhiComparisonFunc_less          = 2,
	RhiComparisonFunc_equal         = 3,
	RhiComparisonFunc_less_equal    = 4,
	RhiComparisonFunc_greater       = 5,
	RhiComparisonFunc_not_equal     = 6,
	RhiComparisonFunc_greater_equal = 7,
	RhiComparisonFunc_always        = 8,
	RhiComparisonFunc_COUNT,
} rhi_comparison_func_t;

typedef enum rhi_fill_mode_t
{
	RhiFillMode_solid     = 0,
	RhiFillMode_wireframe = 1,
	RhiFillMode_COUNT,
} rhi_fill_mode_t;

typedef enum rhi_cull_mode_t
{
	RhiCullMode_none  = 0,
	RhiCullMode_back  = 1,
	RhiCullMode_front = 2,

	RhiCullMode_COUNT,
} rhi_cull_mode_t;

typedef enum rhi_blend_t 
{
	RhiBlend_none             = 0,
	RhiBlend_zero             = 1,
	RhiBlend_one              = 2,
	RhiBlend_src_color        = 3,
	RhiBlend_inv_src_color    = 4,
	RhiBlend_src_alpha        = 5,
	RhiBlend_inv_src_alpha    = 6,
	RhiBlend_dest_alpha       = 7,
	RhiBlend_inv_dest_alpha   = 8,
	RhiBlend_dest_color       = 9,
	RhiBlend_inv_dest_color   = 10,
	RhiBlend_src_alpha_sat    = 11,
	RhiBlend_blend_factor     = 14,
	RhiBlend_inv_blend_factor = 15,
	RhiBlend_src1_color       = 16,
	RhiBlend_inv_src1_color   = 17,
	RhiBlend_src1_alpha       = 18,
	RhiBlend_inv_src1_alpha   = 19,
	RhiBlend_alpha_factor     = 20,
	RhiBlend_inv_alpha_factor = 21,

	RhiBlend_COUNT,
} rhi_blend_t;


typedef enum rhi_blend_op_t
{
	RhiBlendOp_none         = 0,
	RhiBlendOp_add          = 1,
	RhiBlendOp_subtract     = 2,
	RhiBlendOp_rev_subtract = 3,
	RhiBlendOp_min          = 4,
	RhiBlendOp_max          = 5,

	RhiBlendOp_COUNT,
} rhi_blend_op_t;

typedef enum rhi_logic_op_t 
{
	RhiLogicOp_clear         = 0,
	RhiLogicOp_set           = 1,
	RhiLogicOp_copy          = 2,
	RhiLogicOp_copy_inverted = 3,
	RhiLogicOp_noop          = 4,
	RhiLogicOp_invert        = 5,
	RhiLogicOp_and           = 6,
	RhiLogicOp_nand          = 7,
	RhiLogicOp_or            = 8,
	RhiLogicOp_nor           = 9,
	RhiLogicOp_xor           = 10,
	RhiLogicOp_equiv         = 11,
	RhiLogicOp_and_reverse   = 12,
	RhiLogicOp_and_inverted  = 13,
	RhiLogicOp_or_reverse    = 14,
	RhiLogicOp_or_inverted   = 15,
	RhiLogicOp_COUNT,
} rhi_logic_op_t;

enum
{
	RhiColorWriteEnable_red   = 0x1,
	RhiColorWriteEnable_green = 0x2,
	RhiColorWriteEnable_blue  = 0x4,
	RhiColorWriteEnable_alpha = 0x8,
	RhiColorWriteEnable_all   = 0xF,
};

typedef struct rhi_render_target_blend_t
{
	bool blend_enable;
	bool logic_op_enable;

	rhi_blend_t    src_blend;
	rhi_blend_t    dst_blend;
	rhi_blend_op_t blend_op;

	rhi_blend_t    src_blend_alpha;
	rhi_blend_t    dst_blend_alpha;
	rhi_blend_op_t blend_op_alpha;

	rhi_logic_op_t logic_op;

	uint8_t        write_mask;
} rhi_render_target_blend_t;

typedef enum rhi_primitive_topology_type_t
{
	RhiPrimitiveTopologyType_undefined = 0,
	RhiPrimitiveTopologyType_point     = 1,
	RhiPrimitiveTopologyType_line      = 2,
	RhiPrimitiveTopologyType_triangle  = 3,
	RhiPrimitiveTopologyType_patch     = 4,
	RhiPrimitiveTopologyType_COUNT,
} rhi_primitive_topology_type_t;

typedef enum rhi_primitive_topology_t
{
	RhiPrimitiveTopology_undefined         = 0,
	RhiPrimitiveTopology_pointlist         = 1,
	RhiPrimitiveTopology_linelist          = 2,
	RhiPrimitiveTopology_linestrip         = 3,
	RhiPrimitiveTopology_trianglelist      = 4,
	RhiPrimitiveTopology_trianglestrip     = 5,
	RhiPrimitiveTopology_linelist_adj      = 6,
	RhiPrimitiveTopology_linestrip_adj     = 7,
	RhiPrimitiveTopology_trianglelist_adj  = 8,
	RhiPrimitiveTopology_trianglestrip_adj = 9,
	RhiPrimitiveTopology_COUNT,
} rhi_primitive_topology_t;

typedef enum rhi_stencil_op_t 
{
	RhiStencilOp_none     = 0,
	RhiStencilOp_keep     = 1,
	RhiStencilOp_zero     = 2,
	RhiStencilOp_replace  = 3,
	RhiStencilOp_incr_sat = 4,
	RhiStencilOp_decr_sat = 5,
	RhiStencilOp_invert   = 6,
	RhiStencilOp_incr     = 7,
	RhiStencilOp_decr     = 8,
	RhiStencilOp_COUNT,
} rhi_stencil_op_t;

typedef struct rhi_depth_stencil_op_desc_t
{
	rhi_stencil_op_t      stencil_fail_op;
	rhi_stencil_op_t      stencil_depth_fail_op;
	rhi_stencil_op_t      stencil_pass_op;
	rhi_comparison_func_t stencil_func;
} rhi_depth_stencil_op_desc_t;

typedef enum rhi_winding_t
{
	RhiWinding_cw  = 0,
	RhiWinding_ccw = 1,
	RhiWinding_COUNT,
} rhi_winding_t;

typedef struct rhi_create_graphics_pso_params_t
{
	string_t debug_name;

	rhi_shader_bytecode_t vs;
	rhi_shader_bytecode_t ps;

	struct
	{
		bool                      alpha_to_coverage_enable;
		bool                      independent_blend_enable;
		rhi_render_target_blend_t render_target[RhiMaxRenderTargetCount];
		uint32_t                  sample_mask; 
	} blend;

	struct
	{
		rhi_fill_mode_t fill_mode;
		rhi_cull_mode_t cull_mode;
		rhi_winding_t   front_winding;
		bool            depth_clip_enable;
		bool            multisample_enable;
		bool            anti_aliased_line_enable;
		bool            conservative_rasterization;
	} rasterizer;

	struct 
	{
		bool                        depth_test_enable;
		bool                        depth_write;     
		rhi_comparison_func_t       depth_func;
		bool                        stencil_test_enable;
		uint8_t                     stencil_read_mask;                 
		uint8_t                     stencil_write_mask;                
		rhi_depth_stencil_op_desc_t front;
		rhi_depth_stencil_op_desc_t back;
	} depth_stencil;

	rhi_primitive_topology_type_t primitive_topology_type;

	uint32_t           render_target_count;
	pixel_format_t     rtv_formats[RhiMaxRenderTargetCount];
	pixel_format_t     dsv_format;

	uint32_t           multisample_count;
	uint32_t           multisample_quality;
} rhi_create_graphics_pso_params_t;

fn rhi_pso_t rhi_create_graphics_pso(const rhi_create_graphics_pso_params_t *params);

typedef struct rhi_compute_pso_params_t
{
	string_t debug_name;
	rhi_shader_bytecode_t cs;
} rhi_compute_pso_params_t;

fn rhi_pso_t rhi_create_compute_pso(const rhi_compute_pso_params_t *params);

fn void rhi_destroy_pso(rhi_pso_t pso);

typedef enum rhi_pass_op_t
{
	RhiPassOp_none  = 0,
	RhiPassOp_clear = 1,
	RhiPassOp_COUNT,
} rhi_pass_op_t;

typedef struct rhi_viewport_t
{
	float min_x;
	float min_y;
	float width;
	float height;
	float min_depth;
	float max_depth;
} rhi_viewport_t;

typedef struct rhi_graphics_pass_color_attachment_t
{
	rhi_texture_t texture;
	rhi_pass_op_t op;
	v4_t          clear_color;
} rhi_graphics_pass_color_attachment_t;

typedef struct rhi_graphics_pass_depth_stencil_attachment_t
{
	rhi_texture_t texture;
	rhi_pass_op_t op;
	float         clear_value;
} rhi_graphics_pass_depth_stencil_attachment_t;

typedef struct rhi_graphics_pass_params_t
{
	rhi_graphics_pass_color_attachment_t render_targets[RhiMaxRenderTargetCount];
	rhi_graphics_pass_depth_stencil_attachment_t depth_stencil;

	rhi_primitive_topology_t topology;
	rhi_viewport_t           viewport;
	rect2i_t                 scissor_rect;
} rhi_graphics_pass_params_t;

typedef struct rhi_compute_pass_params_t
{
	uint32_t       buffer_uavs_count;
	rhi_buffer_t  *buffer_uavs;

	uint32_t       texture_uavs_count;
	rhi_texture_t *texture_uavs;
} rhi_compute_pass_params_t;

fn void rhi_set_parameters(rhi_command_list_t *list, uint32_t slot, void *parameters, uint32_t size);

fn void                rhi_begin_frame        (void);
fn rhi_command_list_t *rhi_get_command_list   (void);
fn void                rhi_graphics_pass_begin(rhi_command_list_t *list, const rhi_graphics_pass_params_t *params);
fn void                rhi_set_pso            (rhi_command_list_t *list, rhi_pso_t pso);
fn void                rhi_draw               (rhi_command_list_t *list, uint32_t vertex_count, uint32_t start_vertex);
fn void                rhi_draw_indexed       (rhi_command_list_t *list, rhi_buffer_t index_buffer_handle, uint32_t index_count, uint32_t start_index, uint32_t start_vertex);
fn void                rhi_draw_instanced     (rhi_command_list_t *list, uint32_t vertex_count, uint32_t start_vertex, uint32_t instance_count, uint32_t start_instance);
fn void                rhi_graphics_pass_end  (rhi_command_list_t *list);
fn void                rhi_compute_pass_begin (rhi_command_list_t *list, const rhi_compute_pass_params_t *params);
fn void                rhi_dispatch           (rhi_command_list_t *list, int dispatch_x, int dispatch_y, int dispatch_z);
fn void                rhi_compute_pass_end   (rhi_command_list_t *list);
fn void                rhi_end_frame          (void);

fn void rhi_flush_everything(void);

fn void rhi_begin_region(rhi_command_list_t *list, string_t region);
fn void rhi_end_region  (rhi_command_list_t *list);
fn void rhi_marker      (rhi_command_list_t *list, string_t marker);

fn void      rhi_init_timestamps        (uint32_t max_timestamps_per_frame_count);
fn void      rhi_record_timestamp       (rhi_command_list_t *list, uint32_t index);
fn uint64_t  rhi_get_timestamp_frequency(void);
fn uint64_t *rhi_begin_read_timestamps  (void);
fn void      rhi_end_read_timestamps    (void);

fn_local void rhi_simple_graphics_pass_begin(rhi_command_list_t *list, 
											 rhi_texture_t render_target,
											 rhi_texture_t depth_stencil,
											 rhi_primitive_topology_t topology)
{
	const rhi_texture_desc_t *rt_desc = rhi_get_texture_desc(render_target);

	rhi_graphics_pass_begin(list, &(rhi_graphics_pass_params_t){
		.render_targets[0] = { .texture = render_target, },
		.depth_stencil     = depth_stencil,
		.topology          = topology,
		.viewport = {
			.width  = (float)rt_desc->width,
			.height = (float)rt_desc->height,
			.min_depth = 0.0f,
			.max_depth = 1.0f,
		},
		.scissor_rect = {
			.max = { (int32_t)rt_desc->width, (int32_t)rt_desc->height },
		},
	});
}
