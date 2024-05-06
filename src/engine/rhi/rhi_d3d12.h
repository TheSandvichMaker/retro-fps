// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#if !PLATFORM_WIN32
#error D3D12 is only supported on windows!
#endif

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12.h>

#include "rhi_api.h"
#include "d3d12_helpers.h"
#include "d3d12_buffer_arena.h"
#include "d3d12_constants.h"

enum { RhiMaxDescriptors = 4096 };

typedef enum d3d12_bindless_root_parameters_t
{
	D3d12BindlessRootParameter_32bitconstants = 0,
	D3d12BindlessRootParameter_pass_cbv       = 1,
	D3d12BindlessRootParameter_view_cbv       = 2,
} d3d12_bindless_root_parameters_t;

typedef struct d3d12_descriptor_arena_t
{
	ID3D12DescriptorHeap *heap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_base;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_base;
	uint32_t at;
	uint32_t capacity;
	uint32_t stride;
} d3d12_descriptor_arena_t;

typedef struct d3d12_descriptor_t
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	uint32_t stride;
	uint32_t index;
} d3d12_descriptor_t;

typedef struct d3d12_descriptor_range_t
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	uint32_t count;
	uint32_t stride;
	uint32_t index;
} d3d12_descriptor_range_t;

fn void d3d12_descriptor_arena_init(d3d12_descriptor_arena_t *arena, 
									D3D12_DESCRIPTOR_HEAP_TYPE type, 
									UINT capacity, 
									bool shader_visible);
fn void d3d12_descriptor_arena_release(d3d12_descriptor_arena_t *arena);
fn d3d12_descriptor_t d3d12_descriptor_arena_allocate(d3d12_descriptor_arena_t *arena);
fn d3d12_descriptor_range_t d3d12_descriptor_arena_allocate_range(d3d12_descriptor_arena_t *arena, uint32_t count);

typedef struct rhi_command_list_t
{
	ID3D12GraphicsCommandList *d3d;

	uint8_t render_target_mask;
	rhi_texture_t render_targets[8];
} rhi_command_list_t;

typedef struct d3d12_frame_state_t
{
	ID3D12CommandAllocator *command_allocator;
	rhi_command_list_t      command_list;

	d3d12_buffer_arena_t upload_arena;
} d3d12_frame_state_t;

typedef struct rhi_state_d3d12_t
{
	bool initialized;
	bool debug_layer_enabled;
	bool gpu_based_validation_enabled;

	arena_t arena;

	IDXGIFactory6 *dxgi_factory;
	IDXGIAdapter1 *dxgi_adapter;
	ID3D12Device  *device;

	uint64_t     fence_value;
	ID3D12Fence *fence;
	HANDLE       fence_event;

	ID3D12CommandQueue *command_queue;

	d3d12_frame_state_t *frames[3];
	uint32_t frame_buffer_count;
	uint32_t frame_index;

	ID3D12RootSignature *rs_bindless;

	d3d12_descriptor_arena_t cbv_srv_uav;

	pool_t windows;
	pool_t buffers;
	pool_t textures;
	pool_t psos;

	struct
	{
		ID3D12PipelineState *pso;

		ID3D12Resource *positions;
		ID3D12Resource *colors;

		d3d12_descriptor_t desc_positions;
		d3d12_descriptor_t desc_colors;
	} test;
} rhi_state_d3d12_t;

global rhi_state_d3d12_t g_rhi;

typedef struct rhi_init_params_d3d12_t
{
	rhi_init_params_t base;

	bool enable_debug_layer;
	bool enable_gpu_based_validation;
} rhi_init_params_d3d12_t;

typedef struct d3d12_window_t
{
	uint32_t w, h;
	uint32_t backbuffer_index;

	HWND hwnd;
	IDXGISwapChain4         *swap_chain;
	d3d12_descriptor_arena_t rtv_arena;
	rhi_texture_t            frame_buffers[3];
} d3d12_window_t;

typedef struct d3d12_buffer_t
{
	ID3D12Resource *resource;
	d3d12_descriptor_t srv;
	d3d12_descriptor_t uav;
} d3d12_buffer_t;

typedef struct d3d12_texture_t
{
	ID3D12Resource    *resource;
	d3d12_descriptor_t srv;
	d3d12_descriptor_t uav;
	d3d12_descriptor_t rtv;

	rhi_texture_desc_t desc;
} d3d12_texture_t;

typedef struct d3d12_pso_t
{
	ID3D12PipelineState *d3d;
} d3d12_pso_t;

fn bool         rhi_init_d3d12(const rhi_init_params_d3d12_t *params);
fn rhi_window_t rhi_init_window_d3d12(HWND hwnd);
fn bool         rhi_init_test_window_resources(void);
// fn void         rhi_draw_test_window(rhi_window_t window);

