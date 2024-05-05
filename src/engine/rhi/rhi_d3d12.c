//
// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#include "dxc.h"

#include "d3d12_helpers.c"
#include "d3d12_buffer_arena.c"

void d3d12_descriptor_arena_init(d3d12_descriptor_arena_t *arena, 
								D3D12_DESCRIPTOR_HEAP_TYPE type, 
								UINT capacity, 
								bool shader_visible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type           = type,
		.NumDescriptors = capacity,
		.Flags          = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : 0,
	};

	HRESULT hr;

	hr = ID3D12Device_CreateDescriptorHeap(g_rhi.device, &desc, &IID_ID3D12DescriptorHeap, &arena->heap);
	D3D12_CHECK_HR(hr, return); // TODO: think about proper error (non-)recovery

	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(arena->heap, &arena->cpu_base);

	if (shader_visible)
	{
		ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(arena->heap, &arena->gpu_base);
	}

	arena->at       = 1; // 0 is reserved to indicate a null descriptor
	arena->capacity = capacity;
	arena->stride   = ID3D12Device_GetDescriptorHandleIncrementSize(g_rhi.device, type);
}

void d3d12_descriptor_arena_release(d3d12_descriptor_arena_t *arena)
{
	COM_SAFE_RELEASE(arena->heap);
}

d3d12_descriptor_t d3d12_descriptor_arena_allocate(d3d12_descriptor_arena_t *arena)
{
	d3d12_descriptor_t result = {0};

	if (ALWAYS(arena->at < arena->capacity))
	{
		const uint32_t at     = arena->at;
		const uint32_t stride = arena->stride;
		const uint32_t offset = at * stride;

		result = (d3d12_descriptor_t){
			.cpu    = { arena->cpu_base.ptr + offset },
			.gpu    = { arena->gpu_base.ptr + offset },
			.index  = at,
		};

		arena->at += 1;
	}

	return result;
}

d3d12_descriptor_range_t d3d12_descriptor_arena_allocate_range(d3d12_descriptor_arena_t *arena, uint32_t count)
{
	d3d12_descriptor_range_t result = {0};

	if (ALWAYS(arena->at + count <= arena->capacity))
	{
		const uint32_t at     = arena->at;
		const uint32_t stride = arena->stride;
		const uint32_t offset = at * stride;

		result = (d3d12_descriptor_range_t){
			.cpu    = { arena->cpu_base.ptr + offset },
			.gpu    = { arena->gpu_base.ptr + offset },
			.count  = count,
			.stride = stride,
			.index  = at,
		};

		arena->at += 1;
	}

	return result;
}

fn_local D3D12_CPU_DESCRIPTOR_HANDLE d3d12_get_cpu_descriptor_handle(ID3D12DescriptorHeap *heap, 
																	 D3D12_DESCRIPTOR_HEAP_TYPE heap_type, 
																	 size_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE heap_start;
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(heap, &heap_start);

	const uint32_t stride = ID3D12Device_GetDescriptorHandleIncrementSize(g_rhi.device, heap_type);
	D3D12_CPU_DESCRIPTOR_HANDLE result = {
		heap_start.ptr + index * stride,
	};

	return result;
}

fn_local void d3d12_wait_for_frame(void)
{
	uint64_t value = g_rhi.fence_value;

	HRESULT hr = ID3D12CommandQueue_Signal(g_rhi.command_queue, g_rhi.fence, value);
	D3D12_CHECK_HR(hr, return);

	g_rhi.fence_value += 1;

	uint64_t completed = ID3D12Fence_GetCompletedValue(g_rhi.fence);
	if (completed < value)
	{
		hr = ID3D12Fence_SetEventOnCompletion(g_rhi.fence, value, g_rhi.fence_event);
		if (FAILED(hr))
		{
			log(RHI_D3D12, Error, "Failed to set event on completion flag: 0x%X\n", hr);
		}
		WaitForSingleObject(g_rhi.fence_event, INFINITE);
	}
}

bool rhi_init_d3d12(const rhi_init_params_d3d12_t *params)
{
	g_rhi.windows  = (pool_t)INIT_POOL(d3d12_window_t);
	g_rhi.buffers  = (pool_t)INIT_POOL(d3d12_buffer_t);
	g_rhi.textures = (pool_t)INIT_POOL(d3d12_texture_t);

	//
	//
	//

	HRESULT hr = 0;

	IDXGIFactory6          *dxgi_factory      = NULL;
	IDXGIAdapter1          *dxgi_adapter      = NULL;
	ID3D12Device           *device            = NULL;
	ID3D12CommandQueue     *command_queue     = NULL;
	ID3D12Fence            *fence             = NULL;

	bool result = false;

	//
	// Validate Parameters
	//

	if (params->base.frame_buffer_count == 0 ||
		params->base.frame_buffer_count >  3)
	{
		log(RHI_D3D12, Error, "Invalid frame buffer count %u (should between 1 and 3)", params->base.frame_buffer_count);
		goto bail;
	}

	g_rhi.frame_buffer_count = params->base.frame_buffer_count;

	//
	// Enable Debug Layer
	//

	if (params->enable_debug_layer)
	{
		ID3D12Debug *debug;

		hr = D3D12GetDebugInterface(&IID_ID3D12Debug, &debug);
		D3D12_LOG_FAILURE(hr, "Failed to create ID3D12Debug to enable the D3D12 debug layer");

		if (SUCCEEDED(hr))
		{
			ID3D12Debug_EnableDebugLayer(debug);
			g_rhi.debug_layer_enabled = true;

			if (params->enable_gpu_based_validation)
			{
				ID3D12Debug1 *debug1;

				hr = ID3D12Debug_QueryInterface(debug, &IID_ID3D12Debug1, &debug1);
				D3D12_LOG_FAILURE(hr, "Failed to create ID3D12Debug1 to enable GBV");

				if (SUCCEEDED(hr))
				{
					ID3D12Debug1_SetEnableGPUBasedValidation(debug1, true);
					g_rhi.gpu_based_validation_enabled = true;

					COM_SAFE_RELEASE(debug1);
				}

			}

			COM_SAFE_RELEASE(debug);
		}
	}

	//
	// Create Factory
	//

	UINT create_factory_flags = 0;

	if (params->enable_debug_layer)
	{
		create_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	hr = CreateDXGIFactory2(create_factory_flags, &IID_IDXGIFactory6, &dxgi_factory);
	D3D12_CHECK_HR(hr, goto bail);

	//
	// Create Adapter and Device
	//

	for (uint32_t i = 0; 
		 IDXGIFactory6_EnumAdapterByGpuPreference(dxgi_factory, i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, 
												  &IID_IDXGIAdapter1, &dxgi_adapter) != DXGI_ERROR_NOT_FOUND;
		 i += 1)
	{
		hr = D3D12CreateDevice((IUnknown *)dxgi_adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, &device);

		if (SUCCEEDED(hr))
		{
			break;
		}

		COM_SAFE_RELEASE(dxgi_adapter);
	}

	if (!device)
	{
		log(RHI_D3D12, Error, "Failed to create D3D12 device");
		goto bail;
	}

	if (g_rhi.gpu_based_validation_enabled)
	{
		ID3D12DebugDevice1 *debug_device;

		hr = ID3D12Device_QueryInterface(device, &IID_ID3D12DebugDevice1, &debug_device);
		D3D12_LOG_FAILURE(hr, "Failed to create ID3D12DebugDevice1");

		if (SUCCEEDED(hr))
		{
			// TODO(daniel): I took these from Descent Raytraced, but where did I, or Justin, get them from back then?
			D3D12_DEBUG_DEVICE_GPU_BASED_VALIDATION_SETTINGS gbv_settings = {
				.MaxMessagesPerCommandList = 10,
				.DefaultShaderPatchMode    = D3D12_GPU_BASED_VALIDATION_SHADER_PATCH_MODE_GUARDED_VALIDATION,
				.PipelineStateCreateFlags  = D3D12_GPU_BASED_VALIDATION_PIPELINE_STATE_CREATE_FLAG_FRONT_LOAD_CREATE_GUARDED_VALIDATION_SHADERS,
			};
			hr = ID3D12DebugDevice1_SetDebugParameter(debug_device,
													  D3D12_DEBUG_DEVICE_PARAMETER_GPU_BASED_VALIDATION_SETTINGS,
													  &gbv_settings, sizeof(gbv_settings));
			D3D12_LOG_FAILURE(hr, "Failed to set GBV settings");
		}
	}

	//
	// Set Up Info Queue
	//

	ID3D12InfoQueue *info_queue;

	hr = ID3D12Device_QueryInterface(device, &IID_ID3D12InfoQueue, &info_queue);
	D3D12_LOG_FAILURE(hr, "Failed to create ID3D12InfoQueue");

	if (SUCCEEDED(hr))
	{
		ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		ID3D12InfoQueue_SetBreakOnSeverity(info_queue, D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO,
		};

		D3D12_MESSAGE_ID deny_ids[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			// This is a temporary fix for Windows 11 due to a bug that has not been fixed yet
			// D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
		};

		D3D12_INFO_QUEUE_FILTER filter = {
			.DenyList.NumSeverities = ARRAY_COUNT(severities),
			.DenyList.pSeverityList = severities,
			.DenyList.NumIDs        = ARRAY_COUNT(deny_ids),
			.DenyList.pIDList       = deny_ids,
		};

		hr = ID3D12InfoQueue_PushStorageFilter(info_queue, &filter);
		D3D12_LOG_FAILURE(hr, "Failed to push D3D12InfoQueue storage filter");

		COM_SAFE_RELEASE(info_queue);
	}

	//
	// Create a Command Queue
	//

	{
		D3D12_COMMAND_QUEUE_DESC desc = {
			.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		};

		hr = ID3D12Device_CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue, &command_queue);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	// Create per-frame state
	//

	for (size_t i = 0; i < g_rhi.frame_buffer_count; i++)
	{
		d3d12_frame_state_t *frame = m_alloc_struct(&g_rhi.arena, d3d12_frame_state_t);

		{
			hr = ID3D12Device_CreateCommandAllocator(device, 
													 D3D12_COMMAND_LIST_TYPE_DIRECT, 
													 &IID_ID3D12CommandAllocator, 
													 &frame->command_allocator);
			D3D12_CHECK_HR(hr, goto bail);
		}

		{
			hr = ID3D12Device_CreateCommandList(device,
												0,
												D3D12_COMMAND_LIST_TYPE_DIRECT,
												frame->command_allocator,
												NULL,
												&IID_ID3D12GraphicsCommandList,
												&frame->command_list.d3d);
			D3D12_CHECK_HR(hr, goto bail);

			ID3D12GraphicsCommandList_Close(frame->command_list.d3d);

			m_scoped_temp
			{
				string_t name = string_format(temp, "frame upload arena #%zu", i);
				d3d12_arena_init(device, &frame->upload_arena, MB(128), utf16_from_utf8(temp, name).data);
			}
		}

		// d3d12_descriptor_arena_init(&frame->cbv_srv_uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, RhiMaxDescriptors, true);

		g_rhi.frames[i] = frame;
	}

	//
	// Create fence / event
	//

	g_rhi.fence_value = 0;

	{
		hr = ID3D12Device_CreateFence(device,
									  g_rhi.fence_value,
									  D3D12_FENCE_FLAG_NONE,
									  &IID_ID3D12Fence,
									  &fence);
		D3D12_CHECK_HR(hr, goto bail);

		g_rhi.fence_value += 1;

		g_rhi.fence_event = CreateEvent(0, FALSE, FALSE, NULL);
		if (!g_rhi.fence_event)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			D3D12_CHECK_HR(hr, goto bail);
		}
	}

	//
	// Successfully Initialized D3D12
	//

	g_rhi.dxgi_factory      = dxgi_factory;  dxgi_factory      = NULL;
	g_rhi.dxgi_adapter      = dxgi_adapter;  dxgi_adapter      = NULL;
	g_rhi.device            = device;        device            = NULL;
	g_rhi.command_queue     = command_queue; command_queue     = NULL;
	g_rhi.fence             = fence;         fence             = NULL;

	result = true;
	g_rhi.initialized = true;

	log(RHI_D3D12, Info, "Successfully initialized D3D12");

bail:
	COM_SAFE_RELEASE(dxgi_factory);
	COM_SAFE_RELEASE(dxgi_adapter);
	COM_SAFE_RELEASE(device);
	COM_SAFE_RELEASE(command_queue);
	COM_SAFE_RELEASE(fence);

	return result;
}

bool rhi_init_test_window_resources(void)
{
	bool result = false;

	HRESULT hr;

	ID3D12RootSignature       *root_signature    = NULL;
	ID3D12PipelineState       *pso               = NULL;
	ID3D12Resource            *vertex_buffer     = NULL;

	// TEMPORARY
	d3d12_descriptor_arena_init(&g_rhi.cbv_srv_uav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, RhiMaxDescriptors, true);

	//
	// Create PSO
	//

	{
		// Create root signature

		D3D12_ROOT_PARAMETER1 parameters[] = {
			[D3d12BindlessRootParameter_32bitconstants] = {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
				.Constants = {
					.ShaderRegister = 0,
					.RegisterSpace  = 0,
					.Num32BitValues = 60,
				},
			},
			[D3d12BindlessRootParameter_pass_cbv] = {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
				.Descriptor = {
					.ShaderRegister = 1,
					.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
				},
			},
			[D3d12BindlessRootParameter_view_cbv] = {
				.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
				.Descriptor = {
					.ShaderRegister = 2,
					.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
				},
			},
		};

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1 = {
				.NumParameters = ARRAY_COUNT(parameters),
				.pParameters   = parameters,
				.Flags         = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
			},
		};

		ID3DBlob *serialized_desc = NULL;

		hr = D3D12SerializeVersionedRootSignature(&desc, &serialized_desc, NULL);
		D3D12_CHECK_HR(hr, goto bail);

		hr = ID3D12Device_CreateRootSignature(g_rhi.device,
											  0,
											  ID3D10Blob_GetBufferPointer(serialized_desc),
											  ID3D10Blob_GetBufferSize(serialized_desc),
											  &IID_ID3D12RootSignature,
											  &root_signature);
		ID3D10Blob_Release(serialized_desc);

		D3D12_CHECK_HR(hr, goto bail);
	}

	m_scoped_temp
	{
		string_t shader_source = fs_read_entire_file(temp, S("../src/shaders/bindless_triangle.hlsl"));

		UINT compile_flags = 0;

		if (g_rhi.debug_layer_enabled)
		{
			compile_flags |= D3DCOMPILE_DEBUG;
			compile_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}


		ID3DBlob *error = NULL;

		wchar_t *vs_args[] = { L"-E", L"MainVS", L"-T", L"vs_6_6", L"-WX", L"-Zi", L"-I", L"../src/shaders" };

		ID3DBlob *vs = NULL;
		hr = dxc_compile(shader_source.data, (uint32_t)shader_source.count, vs_args, ARRAY_COUNT(vs_args), &vs, &error);

		if (error && ID3D10Blob_GetBufferSize(error) > 0)
		{
			const char* message = ID3D10Blob_GetBufferPointer(error);

			OutputDebugStringA(message);

			if (!vs)
			{
				FATAL_ERROR("Failed to compile vertex shader:\n\n%s", message);
			}

			COM_SAFE_RELEASE(error);
		}

		wchar_t *ps_args[] = { L"-E", L"MainPS", L"-T", L"ps_6_6", L"-WX", L"-Zi", L"-I", L"../src/shaders" };

		ID3DBlob *ps = NULL;
		hr = dxc_compile(shader_source.data, (uint32_t)shader_source.count, ps_args, ARRAY_COUNT(ps_args), &ps, &error);

		if (error && ID3D10Blob_GetBufferSize(error) > 0)
		{
			const char* message = ID3D10Blob_GetBufferPointer(error);

			OutputDebugStringA(message);

			if (!ps)
			{
				FATAL_ERROR("Failed to compile pixel shader:\n\n%s", message);
			}

			COM_SAFE_RELEASE(error);
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
			.pRootSignature = root_signature,
			.VS = {
				.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs),
				.BytecodeLength  = ID3D10Blob_GetBufferSize(vs),
			},
			.PS = {
				.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps),
				.BytecodeLength  = ID3D10Blob_GetBufferSize(ps),
			},
			.BlendState = {
				.RenderTarget[0] = {
					.BlendEnable           = FALSE,
					.LogicOpEnable         = FALSE,
					.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
				},
			},
			.SampleMask = 0xFFFFFFFF,
			.RasterizerState = {
				.FillMode        = D3D12_FILL_MODE_SOLID,
				.CullMode        = D3D12_CULL_MODE_BACK,
				.DepthClipEnable = TRUE,
			},
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1,
			.RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
			.SampleDesc = { .Count = 1 },
		};

		hr = ID3D12Device_CreateGraphicsPipelineState(g_rhi.device,
													  &pso_desc,
													  &IID_ID3D12PipelineState,
													  &pso);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	// Create Vertex Buffer
	//

#if 0
	{
		v3_t positions[] = {
			{  0.00f,  0.25f * aspect, 0.0f },
			{  0.25f, -0.25f * aspect, 0.0f },
			{ -0.25f, -0.25f * aspect, 0.0f },
		};

		v4_t colors[] = {
			{ 1, 0, 0 },
			{ 0, 1, 0 },
			{ 0, 0, 1 },
		};

		g_rhi.test.positions      = d3d12_create_upload_buffer(g_rhi.device, sizeof(positions), positions);
		g_rhi.test.colors         = d3d12_create_upload_buffer(g_rhi.device, sizeof(colors), colors);
		g_rhi.test.desc_positions = d3d12_descriptor_arena_allocate(&g_rhi.cbv_srv_uav);
		g_rhi.test.desc_colors    = d3d12_descriptor_arena_allocate(&g_rhi.cbv_srv_uav);

		{
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
				.Buffer = {
					.FirstElement        = 0,
					.NumElements         = ARRAY_COUNT(positions),
					.StructureByteStride = sizeof(positions[0]),
				},
			};

			ID3D12Device_CreateShaderResourceView(g_rhi.device, g_rhi.test.positions, &desc, g_rhi.test.desc_positions.cpu);
		}

		{
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
				.Buffer = {
					.FirstElement        = 0,
					.NumElements         = ARRAY_COUNT(colors),
					.StructureByteStride = sizeof(colors[0]),
				},
			};

			ID3D12Device_CreateShaderResourceView(g_rhi.device, g_rhi.test.colors, &desc, g_rhi.test.desc_colors.cpu);
		}
	}
#endif

	//
	//
	//

	g_rhi.rs_bindless = root_signature; root_signature = NULL;
	g_rhi.test.pso    = pso;            pso            = NULL;

	result = true;

bail:
	COM_SAFE_RELEASE(root_signature);
	COM_SAFE_RELEASE(pso);
	COM_SAFE_RELEASE(vertex_buffer);

	return result;
}

rhi_window_t rhi_init_window_d3d12(HWND hwnd)
{
	rhi_window_t result = {0};

	d3d12_window_t *window = pool_add(&g_rhi.windows);
	ASSERT(window);

	HRESULT hr;

	IDXGISwapChain4 *swap_chain = NULL;

	//
	// Initialize swapchain
	//

	DXGI_SWAP_CHAIN_DESC1 desc = {
		.Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc  = { 1, 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 2,
		.Scaling     = DXGI_SCALING_STRETCH,
		.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
	};

	hr = IDXGIFactory6_CreateSwapChainForHwnd(g_rhi.dxgi_factory, (IUnknown*)g_rhi.command_queue, hwnd, &desc, NULL, NULL, (IDXGISwapChain1**)&swap_chain);
	D3D12_CHECK_HR(hr, goto bail);

	IDXGIFactory4_MakeWindowAssociation(g_rhi.dxgi_factory, hwnd, DXGI_MWA_NO_ALT_ENTER);

	//
	// Create RTV Descriptor Heap
	//

	d3d12_descriptor_arena_init(&window->rtv_arena, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_rhi.frame_buffer_count + 1, false);

	//
	// Create RTVs
	//

	{
		for (size_t i = 0; i < g_rhi.frame_buffer_count; i++)
		{
			d3d12_texture_t *frame_buffer = pool_add(&g_rhi.textures);
			frame_buffer->usage_flags |= RhiTextureUsage_render_target;

			hr = IDXGISwapChain1_GetBuffer(swap_chain, (uint32_t)i, &IID_ID3D12Resource, &frame_buffer->resource);
			D3D12_CHECK_HR(hr, goto bail);

			frame_buffer->rtv = d3d12_descriptor_arena_allocate(&window->rtv_arena);
			ID3D12Device_CreateRenderTargetView(g_rhi.device, frame_buffer->resource, NULL, frame_buffer->rtv.cpu);

			window->frame_buffers[i] = CAST_HANDLE(rhi_texture_t, pool_get_handle(&g_rhi.textures, frame_buffer));
		}
	}

	//
	// Success
	//

	{
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);

		window->w          = (uint32_t)(client_rect.right - client_rect.left);
		window->h          = (uint32_t)(client_rect.bottom - client_rect.top);
		window->hwnd       = hwnd;
		window->swap_chain = swap_chain; swap_chain = NULL;
		
		result = CAST_HANDLE(rhi_window_t, pool_get_handle(&g_rhi.windows, window));

		char window_title[512];
		GetWindowTextA(hwnd, window_title, sizeof(window_title));

		log(RHI_D3D12, Info, "Successfully initialized swapchain for window '%s'", window_title);
	}

	//
	//
	//

bail:
	COM_SAFE_RELEASE(swap_chain);

	return result;
}

#if 0
void rhi_draw_test_window(rhi_window_t window_handle)
{
	d3d12_window_t *window = pool_get(&g_rhi.windows, window_handle);

	if (!window) 
	{
		return;
	}

	d3d12_wait_for_frame();
	window->backbuffer_index = IDXGISwapChain3_GetCurrentBackBufferIndex(window->swap_chain);

	d3d12_frame_state_t *frame = g_rhi.frames[g_rhi.frame_index];

	d3d12_arena_reset(&frame->upload_arena);

	ID3D12CommandAllocator *allocator = frame->command_allocator;
	ID3D12CommandAllocator_Reset(allocator);

	ID3D12GraphicsCommandList *list = frame->command_list.d3d;
	ID3D12GraphicsCommandList_Reset(list, allocator, g_rhi.test.pso);

	D3D12_VIEWPORT viewport = {
		.Width  = (float)window->w,
		.Height = (float)window->h,
	};

	D3D12_RECT scissor_rect = {
		.right  = window->w,
		.bottom = window->h,
	};

	ID3D12GraphicsCommandList_SetDescriptorHeaps(list, 1, &g_rhi.cbv_srv_uav.heap);
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, g_rhi.rs_bindless);
	ID3D12GraphicsCommandList_RSSetViewports(list, 1, &viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(list, 1, &scissor_rect);

	{
		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource   = window->frame_buffers[window->backbuffer_index],
				.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
				.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			},
		};

		ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = d3d12_get_cpu_descriptor_handle(window->rtv_heap,
																			 D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
																			 window->backbuffer_index);
	ID3D12GraphicsCommandList_OMSetRenderTargets(list, 1, &rtv_handle, FALSE, NULL);

	float clear_color[] = { 0.05f, 0.05f, 0.05f, 1.0f };
	ID3D12GraphicsCommandList_ClearRenderTargetView(list, rtv_handle, clear_color, 0, NULL);

	d3d12_buffer_allocation_t pass_alloc = d3d12_arena_alloc(&frame->upload_arena, sizeof(shader_parameters_t), 256);

	pass_parameters_t *pass_parameters = pass_alloc.cpu;
	pass_parameters->positions = g_rhi.test.desc_positions.index;
	pass_parameters->colors    = g_rhi.test.desc_colors.index;

	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(list, D3d12BindlessRootParameter_pass_cbv, pass_alloc.gpu);

	static float animation_time = 0.0f;

	const float animation_t = sin_ss(animation_time);
	animation_time += 1.0f / 60.0f;

	shader_parameters_t triangle_parameters[] = {
		{ { animation_t * -0.1,  0.0 }, { 1, 0, 0, 1 } },
		{ { animation_t *  0.1,  0.2 }, { 0, 1, 0, 1 } },
		{ { animation_t * -0.3, -0.3 }, { 0, 0, 1, 1 } },
	};

	for (size_t i = 0; i < ARRAY_COUNT(triangle_parameters); i++)
	{
		const uint32_t constants_count = sizeof(shader_parameters_t) / sizeof(uint32_t);
		ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(list, 
																D3d12BindlessRootParameter_32bitconstants, 
																constants_count, 
																&triangle_parameters[i], 
																0);

		ID3D12GraphicsCommandList_IASetPrimitiveTopology(list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
	}

	{
		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource   = window->frame_buffers[window->backbuffer_index],
				.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
				.StateAfter  = D3D12_RESOURCE_STATE_PRESENT,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			},
		};

		ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
	}

	ID3D12GraphicsCommandList_Close(list);

	ID3D12CommandList *lists[] = { (ID3D12CommandList *)list };
	ID3D12CommandQueue_ExecuteCommandLists(g_rhi.command_queue, 1, lists);

	DXGI_PRESENT_PARAMETERS present_parameters = { 0 };
	IDXGISwapChain1_Present1(window->swap_chain, 1, 0, &present_parameters);
}
#endif

//
// API implementation
//

rhi_texture_t rhi_get_current_backbuffer(rhi_window_t handle)
{
	rhi_texture_t result = {0};

	d3d12_window_t *window = pool_get(&g_rhi.windows, handle);

	if (ALWAYS(window))
	{
		result = window->frame_buffers[window->backbuffer_index];
	}

	return result;
}

rhi_buffer_t rhi_create_buffer(const rhi_create_buffer_params_t *params)
{
	rhi_buffer_t result = {0};

	if (NEVER(!params)) 
	{
		log(RHI_D3D12, Error, "Called rhi_create_buffer without parameters");
		return result;
	}

	d3d12_buffer_t *buffer = pool_add(&g_rhi.buffers);
	result = CAST_HANDLE(rhi_buffer_t, pool_get_handle(&g_rhi.buffers, buffer));

	m_scoped_temp
	{
		string16_t debug_name_wide = {0};

		if (params->debug_name.count > 0)
		{
			debug_name_wide = utf16_from_utf8(temp, params->debug_name);
		}

		buffer->resource = d3d12_create_upload_buffer(g_rhi.device, params->size, NULL, debug_name_wide.data);

		const rhi_initial_data_t *data = &params->initial_data;

		if (data->ptr && ALWAYS(data->offset + data->size <= params->size))
		{
			char *mapped = NULL;

			// TODO: error handling...
			ID3D12Resource_Map(buffer->resource, 0, &(D3D12_RANGE){ 0 }, (void **)&mapped);
			memcpy(mapped + data->offset, data->ptr, data->size);
			ID3D12Resource_Unmap(buffer->resource, 0, NULL);
		}
	}

	if (params->srv)
	{
		d3d12_descriptor_t descriptor = d3d12_descriptor_arena_allocate(&g_rhi.cbv_srv_uav);
		buffer->srv = descriptor;

		{
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
				.Buffer = {
					.FirstElement        = params->srv->first_element,
					.NumElements         = params->srv->element_count,
					.StructureByteStride = params->srv->element_stride,
				},
			};

			ID3D12Device_CreateShaderResourceView(g_rhi.device, buffer->resource, &desc, descriptor.cpu);
		}
	}

	return result;
}

rhi_buffer_srv_t rhi_get_buffer_srv(rhi_buffer_t handle)
{
	rhi_buffer_srv_t result = {0};

	d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);

	if (ALWAYS(buffer))
	{
		result.index = buffer->srv.index;
	}

	return result;
}

void rhi_begin_frame(void)
{
	d3d12_wait_for_frame();

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t *window = it.data;
		window->backbuffer_index = IDXGISwapChain3_GetCurrentBackBufferIndex(window->swap_chain);
	}

	d3d12_frame_state_t *frame = g_rhi.frames[g_rhi.frame_index];

	d3d12_arena_reset(&frame->upload_arena);

	ID3D12CommandAllocator *allocator = frame->command_allocator;
	ID3D12CommandAllocator_Reset(allocator);

	ID3D12GraphicsCommandList *list = frame->command_list.d3d;
	ID3D12GraphicsCommandList_Reset(list, allocator, g_rhi.test.pso);

	ID3D12GraphicsCommandList_SetDescriptorHeaps(list, 1, &g_rhi.cbv_srv_uav.heap);
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, g_rhi.rs_bindless);

	// TODO: move this to begin pass
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// TODO: move this to begin pass

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t *window = it.data;

		D3D12_VIEWPORT viewport = {
			.Width  = (float)window->w,
			.Height = (float)window->h,
		};

		D3D12_RECT scissor_rect = {
			.right  = window->w,
			.bottom = window->h,
		};

		ID3D12GraphicsCommandList_RSSetViewports(list, 1, &viewport);
		ID3D12GraphicsCommandList_RSSetScissorRects(list, 1, &scissor_rect);

		if (window)
		{
			break;
		}
	}
}

rhi_command_list_t *rhi_get_command_list(void)
{
	d3d12_frame_state_t *frame = g_rhi.frames[g_rhi.frame_index];
	return &frame->command_list;
}

void rhi_graphics_pass_begin(rhi_command_list_t *list, const rhi_graphics_pass_params_t *params)
{
	ASSERT_MSG(list->render_target_mask == 0, "You can't begin a graphics pass without ending the previous one!");

	uint32_t active_render_target_count = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE rt_descriptors[8];

	for (size_t i = 0; i < RhiMaxRenderTargetCount; i++)
	{
		rhi_texture_t texture_handle = params->render_targets[i].texture;

		if (RESOURCE_HANDLE_VALID(texture_handle))
		{
			d3d12_texture_t *texture = pool_get(&g_rhi.textures, texture_handle);

			ASSERT(texture);
			ASSERT(texture->usage_flags & RhiTextureUsage_render_target);

			list->render_target_mask |= (1u << i);
			list->render_targets[i] = texture_handle;

			rt_descriptors[active_render_target_count] = texture->rtv.cpu;

			active_render_target_count += 1;
		}
	}

	if (active_render_target_count > 0)
	{
		m_scoped_temp
		{
			D3D12_RESOURCE_BARRIER *barriers = m_alloc_array(temp, active_render_target_count, D3D12_RESOURCE_BARRIER);

			uint32_t render_targets_left = active_render_target_count;

			for (size_t i = 0; i < RhiMaxRenderTargetCount; i++)
			{
				if (!list->render_target_mask & (1u << i))
				{
					continue;
				}

				rhi_texture_t    texture_handle = list->render_targets[i];
				d3d12_texture_t *texture        = pool_get(&g_rhi.textures, texture_handle);

				D3D12_RESOURCE_BARRIER *barrier = &barriers[i];
				barrier->Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier->Transition.pResource   = texture->resource;
				barrier->Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
				barrier->Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
				barrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				render_targets_left -= 1;

				if (render_targets_left == 0)
				{
					break;
				}
			}

			ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, active_render_target_count, barriers);
		}
	}

	ID3D12GraphicsCommandList_OMSetRenderTargets(list->d3d, active_render_target_count, rt_descriptors, FALSE, NULL);

	for (size_t i = 0; i < active_render_target_count; i++)
	{
		if (params->render_targets[i].op == RhiPassOp_clear)
		{
			v4_t clear_color = params->render_targets[i].clear_color;
			ID3D12GraphicsCommandList_ClearRenderTargetView(list->d3d, rt_descriptors[i], &clear_color.e[0], 0, NULL);
		}
	}
}

void *rhi_allocate_parameters_(rhi_command_list_t *list, uint32_t size)
{
	(void)list;

	d3d12_frame_state_t *frame = g_rhi.frames[g_rhi.frame_index];

	d3d12_buffer_allocation_t parameters = d3d12_arena_alloc(&frame->upload_arena, size, 256);
	return parameters.cpu;
}

void rhi_set_parameters(rhi_command_list_t *list, uint32_t slot, void *parameters)
{
	ASSERT(slot >= 1 && slot <= 3);

	d3d12_frame_state_t  *frame = g_rhi.frames[g_rhi.frame_index];
	d3d12_buffer_arena_t *arena = &frame->upload_arena;

	ASSERT((uint64_t)parameters >= (uint64_t)arena->cpu_base &&
		   (uint64_t)parameters <  (uint64_t)arena->cpu_base + arena->capacity);

	uint64_t gpu_address = (uint64_t)parameters - (uint64_t)arena->cpu_base + arena->gpu_base;

	ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(list->d3d, slot, gpu_address);
}

void rhi_set_draw_parameters(rhi_command_list_t *list, void *parameters, uint32_t size)
{
	if (NEVER(size % sizeof(uint32_t) != 0))
	{
		log(RHI_D3D12, Error, "called rhi_set_draw_parameters with a parameter size that is not a multiple of 4, which is not allowed");
		return;
	}

	const uint32_t constants_count = size / sizeof(uint32_t);

	ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(list->d3d, 
															D3d12BindlessRootParameter_32bitconstants, 
															constants_count, 
															parameters, 
															0);
}

void rhi_command_list_draw(rhi_command_list_t *list, uint32_t vertex_count)
{
	ID3D12GraphicsCommandList_DrawInstanced(list->d3d, vertex_count, 1, 0, 0);
}

void rhi_graphics_pass_end(rhi_command_list_t *list)
{
	// TODO: count bits intrinsic?
	uint32_t active_render_target_count = 0;
	for (size_t i = 0; i < RhiMaxRenderTargetCount; i++)
	{
		if (list->render_target_mask & (1u << i))
		{
			active_render_target_count += 1;
		}
	}

	if (active_render_target_count > 0)
	{
		m_scoped_temp
		{
			D3D12_RESOURCE_BARRIER *barriers = m_alloc_array(temp, active_render_target_count, D3D12_RESOURCE_BARRIER);

			uint32_t render_targets_left = active_render_target_count;
			for (size_t i = 0; i < RhiMaxRenderTargetCount; i++)
			{
				if (!list->render_target_mask & (1u << i))
				{
					continue;
				}

				rhi_texture_t    texture_handle = list->render_targets[i];
				d3d12_texture_t *texture        = pool_get(&g_rhi.textures, texture_handle);

				D3D12_RESOURCE_BARRIER *barrier = &barriers[i];
				barrier->Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier->Transition.pResource   = texture->resource;
				barrier->Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				barrier->Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
				barrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				render_targets_left -= 1;

				if (render_targets_left == 0)
				{
					break;
				}
			}

			ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, active_render_target_count, barriers);
		}
	}

	list->render_target_mask = 0;
}

void rhi_end_frame(void)
{
	d3d12_frame_state_t *frame = g_rhi.frames[g_rhi.frame_index];

	ID3D12GraphicsCommandList *list = frame->command_list.d3d;
	ID3D12GraphicsCommandList_Close(list);

	ID3D12CommandList *lists[] = { (ID3D12CommandList *)list };
	ID3D12CommandQueue_ExecuteCommandLists(g_rhi.command_queue, 1, lists);

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t *window = it.data;

		DXGI_PRESENT_PARAMETERS present_parameters = { 0 };
		IDXGISwapChain1_Present1(window->swap_chain, 1, 0, &present_parameters);
	}
}
