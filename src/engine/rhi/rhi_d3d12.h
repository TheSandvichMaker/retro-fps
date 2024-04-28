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

typedef struct rhi_state_d3d12_t
{
	bool initialized;
	bool debug_layer_enabled;
	bool gpu_based_validation_enabled;

	IDXGIFactory6 *dxgi_factory;
	IDXGIAdapter1 *dxgi_adapter;
	ID3D12Device  *device;

	uint64_t     fence_value;
	ID3D12Fence *fence;
	HANDLE       fence_event;

	ID3D12CommandQueue *command_queue;

	struct
	{
		ID3D12CommandAllocator    *command_allocator;
		ID3D12GraphicsCommandList *command_list;
		ID3D12RootSignature       *root_signature;
		ID3D12PipelineState       *pso;
		ID3D12Resource            *vertex_buffer;

		D3D12_VERTEX_BUFFER_VIEW vbv;
	} test;

	uint32_t frame_buffer_count;

	pool_t windows;
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

	HWND hwnd;
	IDXGISwapChain4      *swap_chain;
	ID3D12DescriptorHeap *rtv_heap;
	ID3D12Resource       *frame_buffers[3];

	uint32_t              frame_index;
} d3d12_window_t;

fn bool         rhi_init_d3d12(const rhi_init_params_d3d12_t *params);
fn rhi_window_t rhi_init_window_d3d12(HWND hwnd);
fn bool         rhi_init_test_window_resources(float aspect);
fn void         rhi_draw_test_window(rhi_window_t window);
