// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

enum { RhiMaxRenderTargetCount = 8 };

typedef struct rhi_init_params_t
{
	uint32_t frame_buffer_count;
} rhi_init_params_t;

typedef enum rhi_backend_t
{
	RhiBackend_none,

	RhiBackend_d3d12,

	RhiBackend_count,
} rhi_backend_t;

DEFINE_HANDLE_TYPE(rhi_window_t);
DEFINE_HANDLE_TYPE(rhi_buffer_t);
DEFINE_HANDLE_TYPE(rhi_texture_t);

fn rhi_texture_t rhi_get_current_backbuffer(rhi_window_t window);

//
// resources
//

typedef enum rhi_texture_usage_t
{
	RhiTextureUsage_render_target = 0x1,
	RhiTextureUsage_uav           = 0x2,
	RhiTextureUsage_srv           = 0x4,
} rhi_texture_usage_t;

typedef struct rhi_buffer_srv_t
{
	uint32_t index;
} rhi_buffer_srv_t;

typedef struct rhi_buffer_uav_t
{
	uint32_t index;
} rhi_buffer_uav_t;

typedef struct rhi_initial_data_t
{
	void     *ptr;
	uint32_t  offset;
	uint32_t  size;
} rhi_initial_data_t;

typedef struct rhi_create_buffer_srv_params_t
{
	uint32_t first_element;
	uint32_t element_count;
	uint32_t element_stride;
} rhi_create_buffer_srv_params_t;

typedef struct rhi_create_buffer_params_t
{
	string_t                        debug_name;
	uint32_t                        size;
	rhi_initial_data_t              initial_data;
	rhi_create_buffer_srv_params_t *srv;
} rhi_create_buffer_params_t;

fn rhi_buffer_t     rhi_create_buffer (const rhi_create_buffer_params_t *params);
fn rhi_buffer_srv_t rhi_get_buffer_srv(rhi_buffer_t);

//
// graphics
//

typedef struct rhi_command_list_t rhi_command_list_t;

typedef enum rhi_pass_op_t
{
	RhiPassOp_none,

	RhiPassOp_clear,

	RhiPassOp_count,
} rhi_pass_op_t;

typedef struct rhi_graphics_pass_params_t
{
	struct
	{
		rhi_texture_t texture;
		rhi_pass_op_t op;
		v4_t          clear_color;
	} render_targets[RhiMaxRenderTargetCount];
} rhi_graphics_pass_params_t;

fn void *rhi_allocate_parameters_(rhi_command_list_t *list, uint32_t size);
#define  rhi_allocate_parameters(list, type) rhi_allocate_parameters_(list, sizeof(type))

fn void rhi_set_parameters     (rhi_command_list_t *list, uint32_t slot, void *parameters);
fn void rhi_set_draw_parameters(rhi_command_list_t *list, void *parameters, uint32_t size);

fn void                rhi_begin_frame        (void);
fn rhi_command_list_t *rhi_get_command_list   (void);
fn void                rhi_graphics_pass_begin(rhi_command_list_t *list, const rhi_graphics_pass_params_t *params);
fn void                rhi_command_list_draw  (rhi_command_list_t *list, uint32_t vertex_count);
fn void                rhi_graphics_pass_end  (rhi_command_list_t *list);
fn void                rhi_end_frame          (void);
