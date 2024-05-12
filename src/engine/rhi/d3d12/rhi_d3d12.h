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

#include "engine/rhi/rhi_api.h"

#include "d3d12_helpers.h"
#include "d3d12_buffer_arena.h"
#include "d3d12_descriptor_heap.h" 
#include "d3d12_constants.h"
#include "d3d12_upload.h"

enum { RhiMaxDescriptors  = 4096 };
enum { RhiMaxFrameLatency = 3 };

typedef enum d3d12_bindless_root_parameters_t
{
	D3d12BindlessRootParameter_32bitconstants = 0,
	D3d12BindlessRootParameter_pass_cbv       = 1,
	D3d12BindlessRootParameter_view_cbv       = 2,
} d3d12_bindless_root_parameters_t;

typedef struct rhi_command_list_t
{
	ID3D12GraphicsCommandList *d3d;

	uint32_t render_target_count;
	rhi_texture_t render_targets[8];
} rhi_command_list_t;

typedef struct d3d12_frame_state_t
{
	ID3D12CommandAllocator *direct_command_allocator;
	rhi_command_list_t direct_command_list;

	ID3D12CommandAllocator *copy_command_allocator;
	ID3D12GraphicsCommandList *copy_command_list;

	uint32_t fence_value;

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

	ID3D12CommandQueue *direct_command_queue;
	ID3D12CommandQueue *copy_command_queue;

	d3d12_upload_ring_buffer_t upload_ring_buffer;

	d3d12_frame_state_t *frames[RhiMaxFrameLatency];
	uint32_t frame_latency;
	uint64_t frame_index;

	ID3D12RootSignature *rs_bindless;

	d3d12_descriptor_heap_t cbv_srv_uav;
	d3d12_descriptor_heap_t rtv;
	d3d12_descriptor_heap_t dsv;

	pool_t windows;
	pool_t buffers;
	pool_t textures;
	pool_t psos;
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
	IDXGISwapChain4 *swap_chain;
	rhi_texture_t    frame_buffers[3];
} d3d12_window_t;

// TODO: Deduplicate between buffer and texture?

typedef struct d3d12_buffer_t
{
	D3D12_RESOURCE_STATES state;

	ID3D12Resource *resource;
	d3d12_descriptor_t srv;
	d3d12_descriptor_t uav;

	uint64_t upload_fence_value;
} d3d12_buffer_t;

typedef struct d3d12_texture_t
{
	D3D12_RESOURCE_STATES state;

	ID3D12Resource    *resource;
	d3d12_descriptor_t srv;
	d3d12_descriptor_t uav;
	d3d12_descriptor_t rtv;
	d3d12_descriptor_t dsv;

	uint64_t upload_fence_value;

	rhi_texture_desc_t desc;
} d3d12_texture_t;

typedef struct d3d12_pso_t
{
	ID3D12PipelineState *d3d;
} d3d12_pso_t;

fn bool         rhi_init_d3d12(const rhi_init_params_d3d12_t *params);
fn rhi_window_t rhi_init_window_d3d12(HWND hwnd);
