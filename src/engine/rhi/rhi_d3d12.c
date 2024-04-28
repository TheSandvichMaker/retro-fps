// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

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

fn_local void d3d12_wait_for_frame(d3d12_window_t *window)
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

	window->frame_index = IDXGISwapChain3_GetCurrentBackBufferIndex(window->swap_chain);
}

bool rhi_init_d3d12(const rhi_init_params_d3d12_t *params)
{
	g_rhi.windows = (pool_t)INIT_POOL(d3d12_window_t);

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

	g_rhi.dxgi_factory      = dxgi_factory;      dxgi_factory      = NULL;
	g_rhi.dxgi_adapter      = dxgi_adapter;      dxgi_adapter      = NULL;
	g_rhi.device            = device;            device            = NULL;
	g_rhi.command_queue     = command_queue;     command_queue     = NULL;
	g_rhi.fence             = fence;             fence             = NULL;

	result = true;

	log(RHI_D3D12, Info, "Successfully initialized D3D12");

bail:
	COM_SAFE_RELEASE(dxgi_factory);
	COM_SAFE_RELEASE(dxgi_adapter);
	COM_SAFE_RELEASE(device);
	COM_SAFE_RELEASE(command_queue);
	COM_SAFE_RELEASE(fence);

	return result;
}

bool rhi_init_test_window_resources(float aspect)
{
	bool result = false;

	HRESULT hr;

	ID3D12CommandAllocator *command_allocator = NULL;
	ID3D12GraphicsCommandList      *command_list      = NULL;
	ID3D12RootSignature    *root_signature    = NULL;
	ID3D12PipelineState    *pso               = NULL;
	ID3D12Resource         *vertex_buffer     = NULL;

	//
	// Create Command Allocator
	//

	{
		hr = ID3D12Device_CreateCommandAllocator(g_rhi.device, 
												 D3D12_COMMAND_LIST_TYPE_DIRECT, 
												 &IID_ID3D12CommandAllocator, 
												 &command_allocator);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	// Create PSO
	//

	{
		// Create root signature

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_0,
			.Desc_1_0 = {
				.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
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

	{
		string_t shader_source = S(
			"void main_vs("                                 "\n"
			"	in  float4 in_position  : POSITION0, "      "\n"
			"	in  float4 in_color     : COLOR0,"          "\n"
			"	out float4 out_position : SV_POSITION,"     "\n"
			"	out float4 out_color    : COLOR)"           "\n"
			"{"                                             "\n"
			"	out_position = in_position;"                "\n"
			"	out_color    = in_color;"                   "\n"
			"}"                                             "\n"
			""                                              "\n"
			"float4 main_ps("                               "\n"
			"	in float4 in_position : SV_POSITION,"       "\n"
			"	in float4 in_color    : COLOR) : SV_Target" "\n"
			"{"                                             "\n"
			"	return in_color;"                           "\n"
			"}"                                             "\n");

		UINT compile_flags = 0;

		if (g_rhi.debug_layer_enabled)
		{
			compile_flags |= D3DCOMPILE_DEBUG;
			compile_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}

		ID3DBlob *error = NULL;

		ID3DBlob *vs = NULL;
		hr = D3DCompile(shader_source.data, 
						shader_source.count, 
						NULL, NULL, NULL, 
						"main_vs", "vs_4_0", 
						compile_flags, 
						0, 
						&vs, &error);

		if (FAILED(hr))
		{
			const char* message = ID3D10Blob_GetBufferPointer(error);

			OutputDebugStringA(message);
			FATAL_ERROR("Failed to compile vertex shader:\n\n%s", message);

			COM_SAFE_RELEASE(error);
		}

		ID3DBlob *ps = NULL;
		hr = D3DCompile(shader_source.data, 
						shader_source.count, 
						NULL, NULL, NULL, 
						"main_ps", "ps_4_0", 
						compile_flags, 
						0, 
						&ps, &error);

		if (FAILED(hr))
		{
			const char* message = ID3D10Blob_GetBufferPointer(error);

			OutputDebugStringA(message);
			FATAL_ERROR("Failed to compile pixel shader:\n\n%s", message);

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
			.InputLayout = {
				.pInputElementDescs = (D3D12_INPUT_ELEMENT_DESC[]){
					{
						.SemanticName   = "POSITION",
						.Format         = DXGI_FORMAT_R32G32B32_FLOAT,
						.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
					},
					{
						.SemanticName   = "COLOR",
						.Format         = DXGI_FORMAT_R32G32B32A32_FLOAT,
						.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
					},
				},
				.NumElements = 2,
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
	// Create Command List
	//

	{
		hr = ID3D12Device_CreateCommandList(g_rhi.device,
											0,
											D3D12_COMMAND_LIST_TYPE_DIRECT,
											command_allocator,
											pso,
											&IID_ID3D12GraphicsCommandList,
											&command_list);
		D3D12_CHECK_HR(hr, goto bail);

		ID3D12GraphicsCommandList_Close(command_list);
	}

	//
	// Create Vertex Buffer
	//

	{
		float vertices[] = {
			// pos  						color
			 0.00f,  0.25f * aspect, 0.0f,	1,0,0,0,
			 0.25f, -0.25f * aspect, 0.0f, 	0,1,0,0,
			-0.25f, -0.25f * aspect, 0.0f, 	0,0,1,0,
		};

		D3D12_HEAP_PROPERTIES heap_properties = {
			.Type = D3D12_HEAP_TYPE_UPLOAD,
		};

		D3D12_RESOURCE_DESC desc = {
			.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Width            = sizeof(vertices),
			.Height           = 1,
			.DepthOrArraySize = 1,
			.MipLevels        = 1,
			.SampleDesc       = { .Count = 1, .Quality = 0 },
			.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Format           = DXGI_FORMAT_UNKNOWN,
		};

		hr = ID3D12Device_CreateCommittedResource(g_rhi.device,
												  &heap_properties,
												  D3D12_HEAP_FLAG_NONE,
												  &desc,
												  D3D12_RESOURCE_STATE_GENERIC_READ,
												  NULL,
												  &IID_ID3D12Resource,
												  &vertex_buffer);
		D3D12_CHECK_HR(hr, goto bail);

		void *mapped = NULL;

		hr = ID3D12Resource_Map(vertex_buffer, 0, &(D3D12_RANGE){ 0 }, &mapped);
		D3D12_CHECK_HR(hr, goto bail);

		memcpy(mapped, vertices, sizeof(vertices));

		ID3D12Resource_Unmap(vertex_buffer, 0, NULL);

		D3D12_VERTEX_BUFFER_VIEW vbv = {
			.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(vertex_buffer),
			.StrideInBytes  = sizeof(float) * (3 + 4),
			.SizeInBytes    = sizeof(vertices),
		};

		g_rhi.test.vbv = vbv;
	}

	//
	//
	//

	g_rhi.test.command_allocator = command_allocator; command_allocator = NULL;
	g_rhi.test.command_list = command_list; command_list = NULL;
	g_rhi.test.root_signature = root_signature; root_signature = NULL;
	g_rhi.test.pso = pso; pso = NULL;
	g_rhi.test.vertex_buffer = vertex_buffer; vertex_buffer = NULL;

	result = true;

bail:
	COM_SAFE_RELEASE(command_allocator);
	COM_SAFE_RELEASE(command_list);
	COM_SAFE_RELEASE(root_signature);
	COM_SAFE_RELEASE(pso);
	COM_SAFE_RELEASE(vertex_buffer);

	return result;
}

rhi_window_t rhi_init_window_d3d12(HWND hwnd)
{
	rhi_window_t result = {0};

	HRESULT hr;

	IDXGISwapChain4      *swap_chain       = NULL;
	ID3D12DescriptorHeap *rtv_heap         = NULL;
	ID3D12Resource       *frame_buffers[3] = {NULL};

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

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {
			.NumDescriptors = g_rhi.frame_buffer_count,
			.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		};

		hr = ID3D12Device_CreateDescriptorHeap(g_rhi.device, &desc, &IID_ID3D12DescriptorHeap, &rtv_heap);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	// Create RTVs
	//

	{
		const uint32_t rtv_descriptor_stride = 
			ID3D12Device_GetDescriptorHandleIncrementSize(g_rhi.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_descriptor_handle =
			d3d12_get_cpu_descriptor_handle(rtv_heap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 0);

		for (size_t i = 0; i < g_rhi.frame_buffer_count; i++)
		{
			hr = IDXGISwapChain1_GetBuffer(swap_chain, (uint32_t)i, &IID_ID3D12Resource, &frame_buffers[i]);
			D3D12_CHECK_HR(hr, goto bail);

			ID3D12Device_CreateRenderTargetView(g_rhi.device, frame_buffers[i], NULL, rtv_descriptor_handle);
			rtv_descriptor_handle.ptr += rtv_descriptor_stride;
		}
	}

	//
	// Success
	//

	{
		RECT client_rect;
		GetClientRect(hwnd, &client_rect);

		d3d12_window_t *window = pool_add(&g_rhi.windows);
		window->w          = (uint32_t)(client_rect.right - client_rect.left);
		window->h          = (uint32_t)(client_rect.bottom - client_rect.top);
		window->hwnd       = hwnd;
		window->swap_chain = swap_chain; swap_chain = NULL;
		window->rtv_heap   = rtv_heap;   rtv_heap   = NULL;
		
		for (size_t i = 0; i < g_rhi.frame_buffer_count; i++)
		{
			window->frame_buffers[i] = frame_buffers[i];
			frame_buffers[i] = NULL;
		}

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
	COM_SAFE_RELEASE(rtv_heap);

	for (size_t i = 0; i < g_rhi.frame_buffer_count; i++)
	{
		COM_SAFE_RELEASE(frame_buffers[i]);
	}

	return result;
}

void rhi_draw_test_window(rhi_window_t window_handle)
{
	d3d12_window_t *window = pool_get(&g_rhi.windows, window_handle);

	if (!window) 
	{
		return;
	}

	d3d12_wait_for_frame(window);

	ID3D12CommandAllocator_Reset(g_rhi.test.command_allocator);

	ID3D12GraphicsCommandList *list = g_rhi.test.command_list;

	ID3D12GraphicsCommandList_Reset(list, g_rhi.test.command_allocator, g_rhi.test.pso);

	D3D12_VIEWPORT viewport = {
		.Width  = (float)window->w,
		.Height = (float)window->h,
	};

	D3D12_RECT scissor_rect = {
		.right  = window->w,
		.bottom = window->h,
	};

	ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, g_rhi.test.root_signature);
	ID3D12GraphicsCommandList_RSSetViewports(list, 1, &viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(list, 1, &scissor_rect);

	{
		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource   = window->frame_buffers[window->frame_index],
				.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
				.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			},
		};

		ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = d3d12_get_cpu_descriptor_handle(window->rtv_heap,
																			 D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
																			 window->frame_index);
	ID3D12GraphicsCommandList_OMSetRenderTargets(list, 1, &rtv_handle, FALSE, NULL);

	float clear_color[] = { 0.05f, 0.05f, 0.05f, 1.0f };
	ID3D12GraphicsCommandList_ClearRenderTargetView(list, rtv_handle, clear_color, 0, NULL);

	ID3D12GraphicsCommandList_IASetPrimitiveTopology(list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12GraphicsCommandList_IASetVertexBuffers(list, 0, 1, &g_rhi.test.vbv);
	ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);

	{
		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource   = window->frame_buffers[window->frame_index],
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
