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

bool rhi_init_d3d12(const rhi_init_params_d3d12_t *params)
{
	g_rhi.windows = (pool_t)INIT_POOL(d3d12_window_t);

	//
	//
	//

	HRESULT hr = 0;

	IDXGIFactory6      *dxgi_factory  = NULL;
	IDXGIAdapter1      *dxgi_adapter  = NULL;
	ID3D12Device       *device        = NULL;
	ID3D12CommandQueue *command_queue = NULL;

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
	// Successfully Initialized D3D12
	//

	g_rhi.dxgi_factory  = dxgi_factory;  dxgi_factory  = NULL;
	g_rhi.dxgi_adapter  = dxgi_adapter;  dxgi_adapter  = NULL;
	g_rhi.device        = device;        device        = NULL;
	g_rhi.command_queue = command_queue; command_queue = NULL;

	result = true;

	log(RHI_D3D12, Info, "Successfully initialized D3D12");

bail:
	COM_SAFE_RELEASE(dxgi_factory);
	COM_SAFE_RELEASE(dxgi_adapter);
	COM_SAFE_RELEASE(device);
	COM_SAFE_RELEASE(command_queue);

	return result;
}

rhi_window_t rhi_init_window_d3d12(HWND hwnd)
{
	rhi_window_t result = {0};

	HRESULT hr;

	IDXGISwapChain1      *swap_chain       = NULL;
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

	hr = IDXGIFactory6_CreateSwapChainForHwnd(g_rhi.dxgi_factory, (IUnknown*)g_rhi.command_queue, hwnd, &desc, NULL, NULL, &swap_chain);
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
		d3d12_window_t *window = pool_add(&g_rhi.windows);
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
