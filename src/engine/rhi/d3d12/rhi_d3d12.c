// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

// Agility SDK...

extern __declspec(dllexport) const uint32_t D3D12SDKVersion = 613;
extern __declspec(dllexport) const char    *D3D12SDKPath    = u8".\\D3D12\\";

#include "dxc.h"

#include "d3d12_helpers.c"
#include "d3d12_buffer_arena.c"
#include "d3d12_descriptor_heap.c" 
#include "d3d12_constants.c"
#include "d3d12_upload.c"

CVAR_BOOL(cvar_d3d12_force_heavy_synchronization, "rhi.d3d12.force_heavy_synchronization", false);
CVAR_BOOL(cvar_d3d12_timestamp_queries_enabled,   "rhi.d3d12.timestamp_queries_enabled",   true);

fn_local d3d12_frame_state_t *d3d12_get_frame_state(uint32_t frame_index)
{
	return g_rhi.frames[frame_index % g_rhi.frame_latency];
}

fn_local void d3d12_wait_for_frame(uint64_t fence_value)
{
	uint64_t completed = ID3D12Fence_GetCompletedValue(g_rhi.fence);
	if (completed < fence_value)
	{
		HRESULT hr = ID3D12Fence_SetEventOnCompletion(g_rhi.fence, fence_value, g_rhi.fence_event);
		if (FAILED(hr))
		{
			log(RHI_D3D12, Error, "Failed to set event on completion flag: 0x%X\n", hr);
		}
		WaitForSingleObject(g_rhi.fence_event, INFINITE);
	}
}

fn_local bool d3d12_transition_state(ID3D12Resource *resource, 
									 D3D12_RESOURCE_STATES *dst_state, 
									 D3D12_RESOURCE_STATES desired_state, 
									 D3D12_RESOURCE_BARRIER *barrier)
{
	ASSERT(resource);
	ASSERT(dst_state);
	ASSERT(barrier);

	bool result = false;
	if (*dst_state != desired_state)
	{
		*barrier = (D3D12_RESOURCE_BARRIER){
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition = {
				.pResource   = resource,
				.StateBefore = *dst_state,
				.StateAfter  = desired_state,
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			},
		};

		*dst_state = desired_state;

		result = true;
	}

	return result;
}

fn_local void d3d12_uav_barrier(
	ID3D12Resource         *resource, 
	D3D12_RESOURCE_BARRIER *barrier
) {
	ASSERT(resource);
	ASSERT(barrier);

	*barrier = (D3D12_RESOURCE_BARRIER){
		.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
		.UAV = {
			.pResource = resource,
		},
	};
}

bool rhi_init_d3d12(const rhi_init_params_d3d12_t *params)
{
	// Register cvars (optional - but prevents lazy initialization which is preferable)

	cvar_register(&cvar_d3d12_force_heavy_synchronization);
	cvar_register(&cvar_d3d12_timestamp_queries_enabled);

	// initialize pools

	g_rhi.windows  = (pool_t)INIT_POOL_EX(d3d12_window_t,  POOL_FLAGS_CONCURRENT);
	g_rhi.buffers  = (pool_t)INIT_POOL_EX(d3d12_buffer_t,  POOL_FLAGS_CONCURRENT);
	g_rhi.textures = (pool_t)INIT_POOL_EX(d3d12_texture_t, POOL_FLAGS_CONCURRENT);
	g_rhi.psos     = (pool_t)INIT_POOL_EX(d3d12_pso_t,     POOL_FLAGS_CONCURRENT);

	//
	//
	//

	HRESULT hr = 0;

	IDXGIFactory6       *dxgi_factory   = NULL;
	IDXGIAdapter1       *dxgi_adapter   = NULL;
	ID3D12Device        *device         = NULL;
	ID3D12CommandQueue  *direct_queue   = NULL;
	ID3D12CommandQueue  *copy_queue     = NULL;
	ID3D12Fence         *fence          = NULL;
	ID3D12RootSignature *root_signature = NULL;

	bool result = false;

	//
	// Validate Parameters
	//

	if (params->base.frame_latency == 0 ||
		params->base.frame_latency > RhiMaxFrameLatency)
	{
		log(RHI_D3D12, Error, "Invalid frame latency %u (should between 1 and 3)", params->base.frame_latency);
		goto bail;
	}

	g_rhi.frame_latency = params->base.frame_latency;

	//
	// Read QPC freq
	//

	LARGE_INTEGER qpc_freq;
	QueryPerformanceFrequency(&qpc_freq);

	g_rhi.qpc_freq = qpc_freq.QuadPart;

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

            log(RHI_D3D12, Info, "Initialized D3D12 Debug Layer");

			if (params->enable_gpu_based_validation)
			{
				ID3D12Debug1 *debug1;

				hr = ID3D12Debug_QueryInterface(debug, &IID_ID3D12Debug1, &debug1);
				D3D12_LOG_FAILURE(hr, "Failed to create ID3D12Debug1 to enable GBV");

				if (SUCCEEDED(hr))
				{
					ID3D12Debug1_SetEnableGPUBasedValidation(debug1, true);
					g_rhi.gpu_based_validation_enabled = true;

                    log(RHI_D3D12, Info, "Initialized D3D12 GBV");

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

    if (params->enable_debug_layer)
    {
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
    }

	//
	// Create the direct command queue
	//

	{
		D3D12_COMMAND_QUEUE_DESC desc = {
			.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		};

		hr = ID3D12Device_CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue, &direct_queue);
		D3D12_CHECK_HR(hr, goto bail);

		d3d12_set_debug_name((ID3D12Object *)direct_queue, S("direct_queue"));
	}

	//
	// Create the per-frame copy command queue
	//

	{
		D3D12_COMMAND_QUEUE_DESC desc = {
			.Type     = D3D12_COMMAND_LIST_TYPE_COPY,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		};

		hr = ID3D12Device_CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue, &copy_queue);
		D3D12_CHECK_HR(hr, goto bail);

		d3d12_set_debug_name((ID3D12Object *)copy_queue, S("copy_queue"));
	}

	//
	// Create per-frame state
	//

	for (size_t i = 0; i < g_rhi.frame_latency; i++)
	{
		d3d12_frame_state_t *frame = m_alloc_struct(&g_rhi.arena, d3d12_frame_state_t);

		// direct command list
		{
			hr = ID3D12Device_CreateCommandAllocator(device, 
													 D3D12_COMMAND_LIST_TYPE_DIRECT, 
													 &IID_ID3D12CommandAllocator, 
													 &frame->direct_allocator);
			D3D12_CHECK_HR(hr, goto bail);

			hr = ID3D12Device_CreateCommandList(device,
												0,
												D3D12_COMMAND_LIST_TYPE_DIRECT,
												frame->direct_allocator,
												NULL,
												&IID_ID3D12GraphicsCommandList,
												&frame->direct_command_list.d3d);
			D3D12_CHECK_HR(hr, goto bail);

			ID3D12GraphicsCommandList_Close(frame->direct_command_list.d3d);
		}

		// copy command list
		{
			hr = ID3D12Device_CreateCommandAllocator(device, 
													 D3D12_COMMAND_LIST_TYPE_COPY, 
													 &IID_ID3D12CommandAllocator, 
													 &frame->copy_allocator);
			D3D12_CHECK_HR(hr, goto bail);

			hr = ID3D12Device_CreateCommandList(device,
												0,
												D3D12_COMMAND_LIST_TYPE_COPY,
												frame->copy_allocator,
												NULL,
												&IID_ID3D12GraphicsCommandList,
												&frame->copy_command_list);
			D3D12_CHECK_HR(hr, goto bail);

			ID3D12GraphicsCommandList_Close(frame->copy_command_list);
		}

		d3d12_arena_init(device, &frame->upload_arena, MB(128), Sf("frame upload arena #%zu", i));

		g_rhi.frames[i] = frame;
	}

	//
	// Create bindless root signature
	//

	{
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

		D3D12_STATIC_SAMPLER_DESC static_samplers[] = {
			[0] = { // s_aniso_wrap
				.Filter           = D3D12_FILTER_ANISOTROPIC,
				.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MipLODBias       = 0.0f,
				.MaxAnisotropy    = 16,
				.MinLOD           = 0.0f,
				.MaxLOD           = D3D12_FLOAT32_MAX,
				.ShaderRegister   = 0,
				.RegisterSpace    = 100,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			},
			[1] = { // s_aniso_clamped
				.Filter           = D3D12_FILTER_ANISOTROPIC,
				.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.MipLODBias       = 0.0f,
				.MaxAnisotropy    = 16,
				.MinLOD           = 0.0f,
				.MaxLOD           = D3D12_FLOAT32_MAX,
				.ShaderRegister   = 1,
				.RegisterSpace    = 100,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			},
			[2] = { // s_linear_wrap
				.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
				.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MipLODBias       = 0.0f,
				.MinLOD           = 0.0f,
				.MaxLOD           = D3D12_FLOAT32_MAX,
				.ShaderRegister   = 2,
				.RegisterSpace    = 100,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			},
			[3] = { // s_linear_clamped
				.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
				.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.MipLODBias       = 0.0f,
				.MinLOD           = 0.0f,
				.MaxLOD           = D3D12_FLOAT32_MAX,
				.ShaderRegister   = 3,
				.RegisterSpace    = 100,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			},
			[4] = { // s_point_wrap
				.Filter           = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
				.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				.MipLODBias       = 0.0f,
				.MinLOD           = 0.0f,
				.MaxLOD           = D3D12_FLOAT32_MAX,
				.ShaderRegister   = 4,
				.RegisterSpace    = 100,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			},
			[5] = { // s_point_clamped
				.Filter           = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR,
				.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.MipLODBias       = 0.0f,
				.MinLOD           = 0.0f,
				.MaxLOD           = D3D12_FLOAT32_MAX,
				.ShaderRegister   = 5,
				.RegisterSpace    = 100,
				.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
			},
		};

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1 = {
				.NumParameters     = ARRAY_COUNT(parameters),
				.pParameters       = parameters,
				.NumStaticSamplers = ARRAY_COUNT(static_samplers),
				.pStaticSamplers   = static_samplers,
				.Flags             = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
			},
		};

		ID3DBlob *serialized_desc = NULL;

		hr = D3D12SerializeVersionedRootSignature(&desc, &serialized_desc, NULL);
		D3D12_CHECK_HR(hr, goto bail);

		hr = ID3D12Device_CreateRootSignature(device,
											  0,
											  ID3D10Blob_GetBufferPointer(serialized_desc),
											  ID3D10Blob_GetBufferSize(serialized_desc),
											  &IID_ID3D12RootSignature,
											  &root_signature);
		ID3D10Blob_Release(serialized_desc);

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

		g_rhi.fence_event = CreateEvent(0, FALSE, FALSE, NULL);
		if (!g_rhi.fence_event)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			D3D12_CHECK_HR(hr, goto bail);
		}
	}

	//
	// Copy fence
	//

	g_rhi.copy_fence_value = 0;

	{
		hr = ID3D12Device_CreateFence(device,
									  g_rhi.copy_fence_value,
									  D3D12_FENCE_FLAG_NONE,
									  &IID_ID3D12Fence,
									  &g_rhi.copy_fence);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	//
	//

	const uint32_t persistent_descriptor_capacity = 64 * 1024 - 1;
	const uint32_t transient_descriptor_capacity  = 64 * 1024 - 1;

	d3d12_descriptor_heap_init(device, 
							   &g_rhi.arena,
							   &g_rhi.cbv_srv_uav, 
							   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 
							   persistent_descriptor_capacity, 
							   transient_descriptor_capacity, 
							   true,
							   S("cbv_srv_uav"));

	d3d12_descriptor_heap_init(device, &g_rhi.arena, &g_rhi.rtv, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 512, 512, false, S("rtv"));
	d3d12_descriptor_heap_init(device, &g_rhi.arena, &g_rhi.dsv, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 512, 512, false, S("dsv"));

	if (!d3d12_upload_ring_buffer_init(device, &g_rhi.upload_ring_buffer))
	{
		log(RHI_D3D12, Error, "Failed to create upload ring buffer");
		goto bail;
	}

	//------------------------------------------------------------------------
	// Make command signatures

	{
		D3D12_COMMAND_SIGNATURE_DESC desc = {
			.ByteStride = sizeof(rhi_indirect_draw_t),
			.NumArgumentDescs = 1,
			.pArgumentDescs = (D3D12_INDIRECT_ARGUMENT_DESC[]) {
				{ .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW },
			},
		};
		hr = ID3D12Device_CreateCommandSignature(device, &desc, NULL, &IID_ID3D12CommandSignature, &g_rhi.draw_command_signature);
		D3D12_CHECK_HR(hr, goto bail);
	}

	{
		D3D12_COMMAND_SIGNATURE_DESC desc = {
			.ByteStride = sizeof(rhi_indirect_draw_indexed_t),
			.NumArgumentDescs = 1,
			.pArgumentDescs = (D3D12_INDIRECT_ARGUMENT_DESC[]) {
				{ .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED },
			},
		};
		hr = ID3D12Device_CreateCommandSignature(device, &desc, NULL, &IID_ID3D12CommandSignature, &g_rhi.draw_indexed_command_signature);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	// Successfully Initialized D3D12
	//

	g_rhi.dxgi_factory = dxgi_factory;   dxgi_factory   = NULL;
	g_rhi.dxgi_adapter = dxgi_adapter;   dxgi_adapter   = NULL;
	g_rhi.device       = device;         device         = NULL;
	g_rhi.direct_queue = direct_queue;   direct_queue   = NULL;
	g_rhi.copy_queue   = copy_queue;     copy_queue     = NULL;
	g_rhi.fence        = fence;          fence          = NULL;
	g_rhi.rs_bindless  = root_signature; root_signature = NULL;

	result = true;
	g_rhi.initialized = true;

	log(RHI_D3D12, Info, "Successfully initialized D3D12");

bail:
	COM_SAFE_RELEASE(dxgi_factory);
	COM_SAFE_RELEASE(dxgi_adapter);
	COM_SAFE_RELEASE(device);
	COM_SAFE_RELEASE(fence);
	COM_SAFE_RELEASE(direct_queue);
	COM_SAFE_RELEASE(copy_queue);
	COM_SAFE_RELEASE(root_signature);

	return result;
}

fn_local rhi_texture_desc_t rhi_texture_desc_from_d3d12_resource_desc(const D3D12_RESOURCE_DESC *desc)
{
	rhi_texture_dimension_t dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
	switch (desc->Dimension)
	{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D: { dimension = RhiTextureDimension_1d; } break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D: { dimension = RhiTextureDimension_2d; } break;		
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D: { dimension = RhiTextureDimension_3d; } break;
		INVALID_DEFAULT_CASE;
	}

	rhi_texture_usage_t usage_flags = 0;
	if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)    usage_flags |= RhiTextureUsage_render_target;
	if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)    usage_flags |= RhiTextureUsage_depth_stencil;
	if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) usage_flags |= RhiTextureUsage_uav;
	if (desc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)   usage_flags |= RhiTextureUsage_deny_srv;

	rhi_texture_desc_t result = {
		.dimension    = dimension,
		.width        = (uint32_t)desc->Width,
		.height       = desc->Height,
		.depth        = desc->DepthOrArraySize,
		.mip_levels   = desc->MipLevels,
		.format       = from_dxgi_format(desc->Format),
		.sample_count = desc->SampleDesc.Count,
		.usage_flags  = usage_flags,
	};

	return result;
}

rhi_window_t rhi_init_window_d3d12(const rhi_init_window_d3d12_params_t *params)
{
	rhi_window_t result = {0};

	d3d12_window_t *window = pool_add(&g_rhi.windows);
	ASSERT(window);

	HWND hwnd = params->hwnd;

	HRESULT hr;

	IDXGISwapChain4 *swap_chain = NULL;

	//
	// Initialize swapchain
	//

	DXGI_SWAP_CHAIN_DESC1 desc = {
		.Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc  = { 1, 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = RhiMaxFrameLatency,
		.Scaling     = DXGI_SCALING_STRETCH,
		.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.Flags       = params->create_frame_latency_waitable_object ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0,
	};

	hr = IDXGIFactory6_CreateSwapChainForHwnd(g_rhi.dxgi_factory, (IUnknown*)g_rhi.direct_queue, 
											  hwnd, &desc, NULL, NULL, (IDXGISwapChain1**)&swap_chain);
	D3D12_CHECK_HR(hr, goto bail);

	if (params->create_frame_latency_waitable_object)
	{
		window->frame_latency_waitable_object = IDXGISwapChain2_GetFrameLatencyWaitableObject(swap_chain);
		IDXGISwapChain2_SetMaximumFrameLatency(swap_chain, RhiMaxFrameLatency);
	}

	IDXGIFactory4_MakeWindowAssociation(g_rhi.dxgi_factory, hwnd, DXGI_MWA_NO_ALT_ENTER);

	//
	// Create RTVs
	//

	{
		for (size_t i = 0; i < RhiMaxFrameLatency; i++)
		{
			d3d12_texture_t *frame_buffer = pool_add(&g_rhi.textures);
			frame_buffer->state = D3D12_RESOURCE_STATE_COMMON|D3D12_RESOURCE_STATE_PRESENT;

			rhi_texture_t frame_buffer_handle = CAST_HANDLE(rhi_texture_t, pool_get_handle(&g_rhi.textures, frame_buffer));

			hr = IDXGISwapChain1_GetBuffer(swap_chain, (uint32_t)i, &IID_ID3D12Resource, &frame_buffer->resource);
			D3D12_CHECK_HR(hr, goto bail);

			d3d12_set_debug_name((ID3D12Object *)frame_buffer->resource, Sf("frame_buffer_%zu", i));

			D3D12_RESOURCE_DESC rt_desc;
			ID3D12Resource_GetDesc(frame_buffer->resource, &rt_desc);

			frame_buffer->desc = rhi_texture_desc_from_d3d12_resource_desc(&rt_desc);

			frame_buffer->rtv = d3d12_allocate_descriptor_persistent_for_texture(&g_rhi.rtv, frame_buffer_handle);

			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
				.Format        = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			};

			ID3D12Device_CreateRenderTargetView(g_rhi.device, frame_buffer->resource, &rtv_desc, frame_buffer->rtv.cpu);

			window->frame_buffers[i] = frame_buffer_handle;
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

void rhi_resize_window(rhi_window_t handle, uint32_t new_width, uint32_t new_height)
{
	d3d12_window_t *window = pool_get(&g_rhi.windows, handle);
	ASSERT(window);

	d3d12_flush_direct_command_queue();

	HRESULT hr;

	for (size_t i = 0; i < RhiMaxFrameLatency; i++)
	{
		d3d12_texture_t *frame_buffer = pool_get(&g_rhi.textures, window->frame_buffers[i]);
		ASSERT(frame_buffer);

		COM_SAFE_RELEASE(frame_buffer->resource);
	}

	DXGI_SWAP_CHAIN_DESC desc;
	hr = IDXGISwapChain4_GetDesc(window->swap_chain, &desc);
	ASSERT(SUCCEEDED(hr)); // SERIOUSLY FIX THE ERROR HANDLING...

	hr = IDXGISwapChain4_ResizeBuffers(window->swap_chain, 0, new_width, new_height, desc.BufferDesc.Format, desc.Flags);
	ASSERT(SUCCEEDED(hr)); // SERIOUSLY FIX THE ERROR HANDLING...

	for (size_t i = 0; i < RhiMaxFrameLatency; i++)
	{
		d3d12_texture_t *frame_buffer = pool_get(&g_rhi.textures, window->frame_buffers[i]);
		ASSERT(frame_buffer);

		hr = IDXGISwapChain1_GetBuffer(window->swap_chain, (uint32_t)i, &IID_ID3D12Resource, &frame_buffer->resource);
		ASSERT(SUCCEEDED(hr)); // SERIOUSLY FIX THE ERROR HANDLING...
							   //
		d3d12_set_debug_name((ID3D12Object *)frame_buffer->resource, Sf("frame_buffer_%zu", i));

		D3D12_RESOURCE_DESC rt_desc;
		ID3D12Resource_GetDesc(frame_buffer->resource, &rt_desc);

		frame_buffer->desc = rhi_texture_desc_from_d3d12_resource_desc(&rt_desc);

		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
			.Format        = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		};

		ID3D12Device_CreateRenderTargetView(g_rhi.device, frame_buffer->resource, &rtv_desc, frame_buffer->rtv.cpu);
	}

	window->w = new_width;
	window->h = new_height;
}

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

v2_t rhi_get_window_client_size(rhi_window_t handle)
{
	v2_t result = {0};

	d3d12_window_t *window = pool_get(&g_rhi.windows, handle);

	if (ALWAYS(window))
	{
		result.x = (float)window->w;
		result.y = (float)window->h;
	}

	return result;
}

uint64_t rhi_get_frame_sync_cpu_freq(void)
{
	return g_rhi.qpc_freq;
}

bool rhi_get_frame_statistics(rhi_window_t handle, rhi_frame_statistics_t *statistics)
{
	d3d12_window_t *window = pool_get(&g_rhi.windows, handle);

	if (!window)
	{
		log(RHI_D3D12, Error, "Invalid window passed to rhi_get_frame_statistics");
		return false;
	}

	bool result = false;

	DXGI_FRAME_STATISTICS stats;
	HRESULT hr = IDXGISwapChain_GetFrameStatistics(window->swap_chain, &stats);

	if (SUCCEEDED(hr))
	{
		statistics->sync_cpu_time = stats.SyncQPCTime.QuadPart;
		result = true;
	}

	return result;
}

void rhi_wait_on_swap_chain(rhi_window_t handle)
{
	d3d12_window_t *window = pool_get(&g_rhi.windows, handle);

	if (!window)
	{
		log(RHI_D3D12, Error, "Invalid window passed to rhi_wait_on_swap_chain");
		return;
	}

	if (window->frame_latency_waitable_object)
	{
		WaitForSingleObjectEx(window->frame_latency_waitable_object, INFINITE, FALSE);
	}
}

bool rhi_get_window_fullscreen(rhi_window_t handle)
{
	d3d12_window_t *window = pool_get(&g_rhi.windows, handle);

	if (!window)
	{
		log(RHI_D3D12, Error, "Invalid window passed to rhi_set_window_fullscreen");
		return false;
	}

	return window->fullscreen;
}

void rhi_set_window_fullscreen(rhi_window_t handle, bool fullscreen)
{
	d3d12_window_t *window = pool_get(&g_rhi.windows, handle);

	if (!window)
	{
		log(RHI_D3D12, Error, "Invalid window passed to rhi_set_window_fullscreen");
		return;
	}

	bool changed = window->fullscreen != fullscreen;

	if (changed)
	{
		window->fullscreen = fullscreen;
		IDXGISwapChain_SetFullscreenState(window->swap_chain, fullscreen, NULL);
	}
}

const rhi_texture_desc_t *rhi_get_texture_desc(rhi_texture_t handle)
{
	const rhi_texture_desc_t *result = NULL;

	d3d12_texture_t *texture = pool_get(&g_rhi.textures, handle);
	if (texture)
	{
		result = &texture->desc;
	}

	return result;
}

fn_local void d3d12_upload_texture_data(d3d12_texture_t *texture, const rhi_texture_data_t *data)
{
	ASSERT(data->subresources);
	ASSERT(data->subresource_count <= 1);

	// TODO: Mipmaps upload
	// TODO: 3D textures

	ASSERT(texture->desc.dimension == RhiTextureDimension_2d);

	D3D12_RESOURCE_DESC d3d_desc;
	ID3D12Resource_GetDesc(texture->resource, &d3d_desc);

	uint64_t dst_size;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT dst_layout;
	ID3D12Device_GetCopyableFootprints(g_rhi.device, &d3d_desc, 0, 1, 0, &dst_layout, NULL, NULL, &dst_size);

	m_scoped_temp
	{
		string_t debug_name = d3d12_get_debug_name(temp, (ID3D12Object *)texture->resource);
		log(RHI_D3D12, Spam, "Uploading texture data for texture '%.*s'", Sx(debug_name));
	}

	d3d12_upload_context_t ctx = d3d12_upload_begin(dst_size, 512);

	uint8_t *src = data->subresources[0];
	uint8_t *dst = ctx.pointer;

	size_t src_stride  = data->row_stride;
	size_t dst_stride  = dst_layout.Footprint.RowPitch;
	size_t copy_stride = MIN(src_stride, dst_stride); // TODO: should really be texture width times bytes per pixel

	for (size_t y = 0; y < texture->desc.height; y++)
	{
		copy_memory(dst, src, copy_stride);

		src += src_stride;
		dst += dst_stride;
	}

	D3D12_TEXTURE_COPY_LOCATION dst_loc = {
		.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
		.pResource        = texture->resource,
		.SubresourceIndex = 0,
	};

	D3D12_TEXTURE_COPY_LOCATION src_loc = {
		.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
		.pResource = ctx.buffer,
		.PlacedFootprint = {
			.Offset    = ctx.offset,
			.Footprint = {
				.Format   = dst_layout.Footprint.Format,
				.Width    = dst_layout.Footprint.Width,
				.Height   = dst_layout.Footprint.Height,
				.Depth    = dst_layout.Footprint.Depth,
				.RowPitch = dst_layout.Footprint.RowPitch,
			},
		},
	};

	{
		D3D12_RESOURCE_BARRIER barrier;

		if (d3d12_transition_state(texture->resource, &texture->state, D3D12_RESOURCE_STATE_COPY_DEST, &barrier))
		{
			ID3D12GraphicsCommandList_ResourceBarrier(ctx.command_list, 1, &barrier);
		}
	}

	ID3D12GraphicsCommandList_CopyTextureRegion(ctx.command_list, &dst_loc, 0, 0, 0, &src_loc, NULL);

	{
		D3D12_RESOURCE_BARRIER barrier;

		ASSERT(d3d12_transition_state(texture->resource, &texture->state, D3D12_RESOURCE_STATE_COMMON, &barrier));
		ID3D12GraphicsCommandList_ResourceBarrier(ctx.command_list, 1, &barrier);
	}

	texture->upload_fence_value = d3d12_upload_end(&ctx);
}

void rhi_upload_texture_data(rhi_texture_t handle, const rhi_texture_data_t *data)
{
	d3d12_texture_t *texture = pool_get(&g_rhi.textures, handle);
	ASSERT_MSG(texture, "Invalid texture!");

	d3d12_upload_texture_data(texture, data);
}

bool rhi_texture_upload_complete(rhi_texture_t handle)
{
	d3d12_texture_t *texture = pool_get(&g_rhi.textures, handle);
	ASSERT_MSG(texture, "Invalid texture!");

	uint64_t fence_value = ID3D12Fence_GetCompletedValue(g_rhi.upload_ring_buffer.fence);
	return texture->upload_fence_value <= fence_value;
}

void rhi_wait_on_texture_upload(rhi_texture_t handle)
{
	d3d12_texture_t *texture = pool_get(&g_rhi.textures, handle);
	ASSERT_MSG(texture, "Invalid texture!");

	ID3D12Fence_SetEventOnCompletion(g_rhi.upload_ring_buffer.fence, texture->upload_fence_value, NULL);
}

rhi_texture_t rhi_create_texture(const rhi_create_texture_params_t *params)
{
	ASSERT_MSG(params, "Called rhi_create_texture without any parameters!");

	rhi_texture_t result = {0};

	m_scoped_temp
	{
		D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;
		switch (params->dimension)
		{
			case RhiTextureDimension_unknown: dimension = D3D12_RESOURCE_DIMENSION_UNKNOWN;   break;
			case RhiTextureDimension_1d:      dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D; break;
			case RhiTextureDimension_2d:      dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
			case RhiTextureDimension_3d:      dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D; break;
			INVALID_DEFAULT_CASE;
		}

		// TODO: Maybe be nice and infer the dimension from the width/height/depth provided?
		ASSERT_MSG(dimension != D3D12_RESOURCE_DIMENSION_UNKNOWN, "Did you forget to specify a texture dimension?");

		D3D12_RESOURCE_FLAGS resource_flags = 0;
		if (params->usage & RhiTextureUsage_render_target) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (params->usage & RhiTextureUsage_depth_stencil) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (params->usage & RhiTextureUsage_uav)           resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (params->usage & RhiTextureUsage_deny_srv)      resource_flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		ID3D12Resource *resource = NULL;

		D3D12_RESOURCE_STATES natural_state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

		// TODO: What combinations of usages are allowed? Clean up the resource state situation!
		if (params->usage & RhiTextureUsage_render_target) natural_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (params->usage & RhiTextureUsage_depth_stencil) natural_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		D3D12_RESOURCE_STATES initial_state = params->initial_data ? D3D12_RESOURCE_STATE_COPY_DEST : natural_state;

		D3D12_CLEAR_VALUE clear_value = {
			.Format = to_dxgi_format(params->format),
			.Color  = { params->optimized_clear_value.x, params->optimized_clear_value.y, params->optimized_clear_value.z, params->optimized_clear_value.w },
		};

		bool wants_clear_value = (params->usage & (RhiTextureUsage_render_target|RhiTextureUsage_depth_stencil));

		DXGI_FORMAT format = to_dxgi_format(params->format);

		if ( (params->usage & RhiTextureUsage_depth_stencil) &&
			!(params->usage & RhiTextureUsage_deny_srv))
		{
			if (format == DXGI_FORMAT_D24_UNORM_S8_UINT)
			{
				format = DXGI_FORMAT_R24G8_TYPELESS;
			}
		}

		HRESULT hr = ID3D12Device_CreateCommittedResource(
			g_rhi.device,
			(&(D3D12_HEAP_PROPERTIES){
				.Type = D3D12_HEAP_TYPE_DEFAULT,
			}),
			0,
			(&(D3D12_RESOURCE_DESC){
				.Dimension        = dimension,
				.Width            = params->width,
				.Height           = params->height,
				.DepthOrArraySize = MAX(1, params->depth),
				.MipLevels        = MAX(1, params->mip_levels),
				.Format           = format,
				.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
				.Flags            = resource_flags,
				.SampleDesc       = { .Count = MAX(1, params->multisample_count) },
			}),
			initial_state,
			wants_clear_value ? &clear_value : NULL,
			&IID_ID3D12Resource,
			&resource);

		if (SUCCEEDED(hr))
		{
			d3d12_texture_t *texture = pool_add(&g_rhi.textures);
			texture->state    = initial_state;
			texture->resource = resource;

			result = CAST_HANDLE(rhi_texture_t, pool_get_handle(&g_rhi.textures, texture));

			D3D12_RESOURCE_DESC desc;
			ID3D12Resource_GetDesc(texture->resource, &desc);

			texture->desc = rhi_texture_desc_from_d3d12_resource_desc(&desc);

			if (texture->desc.usage_flags & RhiTextureUsage_render_target)
			{
				texture->rtv = d3d12_allocate_descriptor_persistent_for_texture(&g_rhi.rtv, result);
				ID3D12Device_CreateRenderTargetView(g_rhi.device, texture->resource, NULL, texture->rtv.cpu);
			}

			const bool multisampled = (params->multisample_count > 1);

			if (texture->desc.usage_flags & RhiTextureUsage_depth_stencil)
			{
				texture->dsv = d3d12_allocate_descriptor_persistent_for_texture(&g_rhi.dsv, result);

				DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;
				switch (format)
				{
					case DXGI_FORMAT_R24G8_TYPELESS: dsv_format = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
				}

				ASSERT(params->dimension == RhiTextureDimension_2d); // TODO: other dimensions

				D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
					.Format        = dsv_format,
					.ViewDimension = (multisampled ? 
									  D3D12_DSV_DIMENSION_TEXTURE2DMS : 
									  D3D12_DSV_DIMENSION_TEXTURE2D),
				};

				ID3D12Device_CreateDepthStencilView(g_rhi.device, texture->resource, &dsv_desc, texture->dsv.cpu);
			}

			if (texture->desc.usage_flags & RhiTextureUsage_uav)
			{
				texture->uav = d3d12_allocate_descriptor_persistent_for_texture(&g_rhi.cbv_srv_uav, result);
				ID3D12Device_CreateUnorderedAccessView(g_rhi.device, texture->resource, NULL, NULL, texture->uav.cpu);
			}

			if (!(texture->desc.usage_flags & RhiTextureUsage_deny_srv))
			{
				texture->srv = d3d12_allocate_descriptor_persistent_for_texture(&g_rhi.cbv_srv_uav, result);

				DXGI_FORMAT srv_format = format;
				switch (format)
				{
					case DXGI_FORMAT_R24G8_TYPELESS: srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; break; // TODO: Stencil SRVs
				}

				ASSERT(params->dimension == RhiTextureDimension_2d); // TODO: other dimensions

				D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
					.Format                  = srv_format,
					.ViewDimension           = (multisampled ? 
												D3D12_SRV_DIMENSION_TEXTURE2DMS : 
												D3D12_SRV_DIMENSION_TEXTURE2D),
					.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
					.Texture2D = {
						.MipLevels = (UINT)-1,
					},
				};

				ID3D12Device_CreateShaderResourceView(g_rhi.device, texture->resource, &srv_desc, texture->srv.cpu);
			}

			uint32_t rtv_index = texture->rtv.index;
			uint32_t dsv_index = texture->dsv.index;
			uint32_t uav_index = texture->uav.index;
			uint32_t srv_index = texture->srv.index;

			DEBUG_ASSERT(params->debug_name.count);

			d3d12_set_debug_name((ID3D12Object *)texture->resource, 
				Sf("%cs (rtv: %u, dsv: %u, uav: %u, srv: %u)",
					params->debug_name,
					rtv_index,
					dsv_index,
					uav_index,
					srv_index));


			rhi_texture_data_t *initial_data = params->initial_data;

			if (initial_data)
			{
				d3d12_upload_texture_data(texture, initial_data);
			}

			log(RHI_D3D12, Spam, "Added new texture '%.*s' (index: %u, generation: %u)", Sx(params->debug_name), result.index, result.generation);
		}
	}

	return result;
}

void rhi_destroy_texture(rhi_texture_t handle)
{
	if (RESOURCE_HANDLE_VALID(handle))
	{
		d3d12_texture_t *texture = pool_get(&g_rhi.textures, handle);
		ASSERT(texture);

		d3d12_deferred_release((IUnknown *)texture->resource);

		d3d12_free_descriptor_persistent(&g_rhi.cbv_srv_uav, texture->srv.index);
		d3d12_free_descriptor_persistent(&g_rhi.cbv_srv_uav, texture->uav.index);
		d3d12_free_descriptor_persistent(&g_rhi.rtv, texture->rtv.index);
		d3d12_free_descriptor_persistent(&g_rhi.dsv, texture->dsv.index);

		pool_rem_item(&g_rhi.textures, texture);
	}
}

rhi_texture_srv_t rhi_get_texture_srv(rhi_texture_t handle)
{
	d3d12_texture_t *texture = pool_get(&g_rhi.textures, handle);
	ASSERT(texture);

	rhi_texture_srv_t result = {
		.index = texture->srv.index,
	};

	return result;
}

void rhi_validate_texture_srv(rhi_texture_srv_t srv, string_t context)
{
	(void)srv;
	(void)context;

#if DREAM_SLOW
	d3d12_descriptor_heap_t *heap = &g_rhi.cbv_srv_uav;

	if (srv.index < heap->capacity)
	{
		rhi_texture_t handle = heap->debug_texture_map[srv.index];

		if (RESOURCE_HANDLE_VALID(handle))
		{
			d3d12_texture_t *texture = pool_get(&g_rhi.textures, handle);

			if (texture)
			{
				if (texture->srv.index != srv.index)
				{
					log(RHI_D3D12, ValidationFailure, "SRV is associated with a texture, yet that texture's SRV does not match. (%cs)", context);
				}

				if (!texture->resource)
				{
					log(RHI_D3D12, ValidationFailure, "SRV is associated with a texture, but that texture does not have an associated resource. (%cs)", context);
				}
			}
			else
			{
				log(RHI_D3D12, ValidationFailure, "SRV is associated with a non-existent texture. (%cs)", context);
			}
		}
		else
		{
			log(RHI_D3D12, ValidationFailure, "SRV is not associated with any texture. (%cs)", context);
		}
	}
	else
	{
		log(RHI_D3D12, ValidationFailure, "SRV is out of range of the CBV/SRV/UAV heap! (%cs)", context);
	}
#endif
}

fn_local void d3d12_upload_buffer_data(d3d12_buffer_t *buffer, uint32_t resource_index, uint64_t dst_offset, const void *src, size_t src_size)
{
	ASSERT((buffer->flags & RhiResourceFlag_dynamic) ? resource_index < RhiMaxFrameLatency : resource_index == 0);

	ID3D12Resource *resource = buffer->resources[resource_index];

	m_scoped_temp
	{
		string_t debug_name = d3d12_get_debug_name(temp, (ID3D12Object *)resource);
		log(RHI_D3D12, Spam, "Uploading data for buffer '%.*s'[%u]", Sx(debug_name), resource_index);
	}

	d3d12_upload_context_t ctx = d3d12_upload_begin(src_size, 256);

	{
		D3D12_RESOURCE_BARRIER barrier;

		if (d3d12_transition_state(resource, &buffer->state[resource_index], D3D12_RESOURCE_STATE_COPY_DEST, &barrier))
		{
			ID3D12GraphicsCommandList_ResourceBarrier(ctx.command_list, 1, &barrier);
		}
	}

	memcpy(ctx.pointer, src, src_size);

	ID3D12GraphicsCommandList_CopyBufferRegion(ctx.command_list, 
											   resource, 
											   dst_offset, 
											   ctx.buffer, 
											   ctx.offset, 
											   src_size);

	{
		D3D12_RESOURCE_BARRIER barrier;

		ASSERT(d3d12_transition_state(resource, &buffer->state[resource_index], D3D12_RESOURCE_STATE_COMMON, &barrier));
		ID3D12GraphicsCommandList_ResourceBarrier(ctx.command_list, 1, &barrier);
	}

	buffer->upload_fence_value = d3d12_upload_end(&ctx);
}

void rhi_upload_buffer_data(rhi_buffer_t handle, 
							size_t dst_offset, 
							const void *src,
							size_t src_size,
							rhi_upload_frequency_t frequency)
{
	ASSERT_MSG(src, "You have to pass a copy source!");

	d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);
	ASSERT_MSG(buffer, "Invalid buffer!");

	uint32_t resource_index = d3d12_resource_index(buffer->flags);

	switch (frequency)
	{
		case RhiUploadFreq_frame:
		{
			d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);
			d3d12_buffer_allocation_t alloc = d3d12_arena_alloc(&frame->upload_arena, (uint32_t)src_size, 256);

			memcpy(alloc.cpu, src, src_size);

			ID3D12GraphicsCommandList *copy_list = frame->copy_command_list;
			ID3D12GraphicsCommandList_CopyBufferRegion(copy_list, buffer->resources[resource_index], dst_offset, 
													   alloc.buffer, alloc.offset, src_size);
		} break;

		case RhiUploadFreq_background:
		{
			d3d12_upload_buffer_data(buffer, resource_index, dst_offset, src, src_size);
		} break;
	}
}

void *rhi_begin_buffer_upload(rhi_buffer_t handle, size_t dst_offset, size_t upload_size, rhi_upload_frequency_t frequency)
{
	ASSERT_MSG(upload_size > 0, "You shouldn't try to upload 0 bytes!");

	d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);
	ASSERT_MSG(buffer, "Invalid buffer!");
	ASSERT_MSG(buffer->upload.pending == false, "You can't begin a buffer upload before ending the previous one!");

	// TODO: Allow background frequency
	ASSERT(frequency == RhiUploadFreq_frame);

	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);
	d3d12_buffer_allocation_t alloc = d3d12_arena_alloc(&frame->upload_arena, (uint32_t)upload_size, 256);

	buffer->upload.pending    = true;
	buffer->upload.src_buffer = alloc.buffer;
	buffer->upload.src_offset = alloc.offset;
	buffer->upload.dst_offset = dst_offset;
	buffer->upload.size       = upload_size;

	return alloc.cpu;
}

void rhi_end_buffer_upload(rhi_buffer_t handle)
{
	d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);
	ASSERT_MSG(buffer, "Invalid buffer!");
	ASSERT_MSG(buffer->upload.pending == true, "You can't end a buffer upload without beginning one!");

	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);
	ID3D12GraphicsCommandList *copy_list = frame->copy_command_list;

	uint32_t resource_index = d3d12_resource_index(buffer->flags);
	ID3D12Resource *resource = buffer->resources[resource_index];

	{
		D3D12_RESOURCE_BARRIER barrier;

		if (d3d12_transition_state(resource, &buffer->state[resource_index], D3D12_RESOURCE_STATE_COPY_DEST, &barrier))
		{
			ID3D12GraphicsCommandList_ResourceBarrier(copy_list, 1, &barrier);
		}
	}

	ID3D12GraphicsCommandList_CopyBufferRegion(copy_list, resource, buffer->upload.dst_offset, 
											   buffer->upload.src_buffer, buffer->upload.src_offset, buffer->upload.size);

	{
		D3D12_RESOURCE_BARRIER barrier;

		ASSERT(d3d12_transition_state(resource, &buffer->state[resource_index], D3D12_RESOURCE_STATE_COMMON, &barrier));
		ID3D12GraphicsCommandList_ResourceBarrier(copy_list, 1, &barrier);
	}

	buffer->upload.pending = false;
}

void rhi_wait_on_buffer_upload(rhi_buffer_t handle)
{
	if (RESOURCE_HANDLE_VALID(handle))
	{
		d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);

		if (buffer)
		{
			ID3D12Fence_SetEventOnCompletion(g_rhi.upload_ring_buffer.fence, buffer->upload_fence_value, NULL);
		}
		else
		{
			log(RHI_D3D12, Error, "Passed invalid buffer to rhi_wait_on_buffer_upload");
		}
	}
}

rhi_buffer_t rhi_create_buffer(const rhi_create_buffer_params_t *params)
{
	ASSERT_MSG(params, "Called rhi_create_buffer without any parameters!");

	rhi_buffer_t result = {0};

	size_t size = 0;

	bool is_raw = !!(params->desc.flags & RhiBufferFlag_raw);

	if (is_raw)
	{
		size = params->desc.element_count;
	}
	else
	{
		size = params->desc.element_count*params->desc.element_stride;
	}

	m_scoped_temp
	{
		D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;

		switch (params->heap)
		{
			case RhiHeapKind_default:  heap_type = D3D12_HEAP_TYPE_DEFAULT;  break;
			case RhiHeapKind_upload:   heap_type = D3D12_HEAP_TYPE_UPLOAD;   break;
			case RhiHeapKind_readback: heap_type = D3D12_HEAP_TYPE_READBACK; break;

			default:
			{
				log(RHI_D3D12, Error, "Invalid heap kind passed to rhi_create_buffer: 0x%X", params->heap);
			} break;
		}

		UINT flags = 0;

		if (params->usage & RhiBufferUsage_uav)
		{
			flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		D3D12_HEAP_PROPERTIES heap_properties = {
			.Type = heap_type,
		};

		D3D12_RESOURCE_DESC desc = {
			.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Width            = size,
			.Height           = 1,
			.DepthOrArraySize = 1,
			.MipLevels        = 1,
			.SampleDesc       = { .Count = 1, .Quality = 0 },
			.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Format           = DXGI_FORMAT_UNKNOWN,
			.Flags            = flags,
		};

		uint32_t count = (params->flags & RhiResourceFlag_dynamic) ? g_rhi.frame_latency : 1;

		HRESULT hr;

		bool succeeded = true;

		ID3D12Resource *resources[RhiMaxFrameLatency] = {0};
		for (size_t i = 0; i < count; i++)
		{
			hr = ID3D12Device_CreateCommittedResource(g_rhi.device,
													  &heap_properties,
													  D3D12_HEAP_FLAG_NONE,
													  &desc,
													  D3D12_RESOURCE_STATE_COMMON,
													  NULL,
													  &IID_ID3D12Resource,
													  &resources[i]);
			if (FAILED(hr))
			{
				succeeded = false;
			}
		}

		if (succeeded)
		{
			d3d12_buffer_t *buffer = pool_add(&g_rhi.buffers);
			result = CAST_HANDLE(rhi_buffer_t, pool_get_handle(&g_rhi.buffers, buffer));

			buffer->flags = params->flags;

			for (size_t i = 0; i < count; i++)
			{
				buffer->resources  [i] = resources[i];
				buffer->gpu_address[i] = ID3D12Resource_GetGPUVirtualAddress(resources[i]);
			}

			buffer->size = size;

			if (!(params->usage & RhiBufferUsage_deny_srv))
			{
				for (size_t i = 0; i < count; i++)
				{
					d3d12_descriptor_t descriptor = d3d12_allocate_descriptor_persistent_for_buffer(&g_rhi.cbv_srv_uav, result);
					buffer->srvs[i] = descriptor;

					{
						D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
							.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
							.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
							.Buffer = {
								.FirstElement        = params->desc.first_element,
								.NumElements         = params->desc.element_count,
								.StructureByteStride = params->desc.element_stride,
								.Flags = is_raw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE,
							},
						};

						ID3D12Device_CreateShaderResourceView(g_rhi.device, buffer->resources[i], &desc, descriptor.cpu);
					}
				}
			}

			if (params->usage & RhiBufferUsage_uav)
			{
				for (size_t i = 0; i < count; i++)
				{
					d3d12_descriptor_t descriptor = d3d12_allocate_descriptor_persistent_for_buffer(&g_rhi.cbv_srv_uav, result);
					buffer->uavs[i] = descriptor;

					{
						D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {
							.Format = DXGI_FORMAT_UNKNOWN,
							.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
							.Buffer = {
								.FirstElement         = params->desc.first_element,
								.NumElements          = params->desc.element_count,
								.StructureByteStride  = params->desc.element_stride,
								.CounterOffsetInBytes = 0,
								.Flags = is_raw ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE,
							},
						};

						ID3D12Device_CreateUnorderedAccessView(g_rhi.device, buffer->resources[i], NULL, &desc, descriptor.cpu);
					}
				}
			}

			if (count == 1)
			{
				d3d12_set_debug_name((ID3D12Object *)buffer->resources[0], 
					Sf("%cs (uav: %u, srv: %u)",
						params->debug_name,
						buffer->uavs[0].index,
						buffer->srvs[0].index));
			}
			else
			{
				for (size_t i = 0; i < count; i++)
				{
					d3d12_set_debug_name((ID3D12Object *)buffer->resources[i], 
						Sf("%cs[%zu] (uav: %u, srv: %u)",
							params->debug_name,
							i,
							buffer->uavs[i].index,
							buffer->srvs[i].index));
				}
			}

			const rhi_buffer_data_t *data = &params->initial_data;

			if (data->src && ALWAYS(data->size <= size))
			{
				for (uint32_t i = 0; i < count; i++)
				{
					//------------------------------------------------------------------------
					// Lying about the initial state because I can't set the initial state on
					// the buffer without triggering a debug layer warning about the initial 
					// state being ignored because it's assumed on first use... Ok... That
					// really makes it easier to do the manual state tracking as the user, 
					// for the state to just be ~whatever~ based on what you do with the 
					// resource!
					buffer->state[i] = D3D12_RESOURCE_STATE_COPY_DEST;

					d3d12_upload_buffer_data(buffer, i, 0, params->initial_data.src, params->initial_data.size);
				}
			}
		}
		else
		{
			log(RHI_D3D12, Error, "Failed to create buffer resource(s)");
		}
	}

	return result;
}

rhi_buffer_srv_t rhi_get_buffer_srv(rhi_buffer_t handle)
{
	rhi_buffer_srv_t result = {0};

	d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);
	ASSERT_MSG(buffer, "Buffer does not exist!");

	uint32_t resource_index = d3d12_resource_index(buffer->flags);

	result.index = buffer->srvs[resource_index].index;
	ASSERT_MSG(result.index != 0, "Buffer does not have an SRV!");

	return result;
}

rhi_buffer_uav_t rhi_get_buffer_uav(rhi_buffer_t handle)
{
	rhi_buffer_uav_t result = {0};

	d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);
	ASSERT_MSG(buffer, "Buffer does not exist!");

	uint32_t resource_index = d3d12_resource_index(buffer->flags);

	result.index = buffer->uavs[resource_index].index;
	ASSERT_MSG(result.index != 0, "Buffer does not have a UAV!");

	return result;
}

void rhi_validate_buffer_srv(rhi_buffer_srv_t srv, string_t context)
{
	(void)srv;
	(void)context;

#if DREAM_SLOW
	d3d12_descriptor_heap_t *heap = &g_rhi.cbv_srv_uav;

	if (srv.index < heap->capacity)
	{
		rhi_buffer_t handle = heap->debug_buffer_map[srv.index];

		if (RESOURCE_HANDLE_VALID(handle))
		{
			d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);

			if (buffer)
			{
				uint32_t resource_index = d3d12_resource_index(buffer->flags);

				if (buffer->srvs[resource_index].index != srv.index)
				{
					log(RHI_D3D12, ValidationFailure, "SRV is associated with a buffer, yet that buffer's SRV does not match. (%cs)", context);
				}

				if (!buffer->resources[resource_index])
				{
					log(RHI_D3D12, ValidationFailure, "SRV is associated with a buffer, but that buffer does not have an associated resource. (%cs)", context);
				}
			}
			else
			{
				log(RHI_D3D12, ValidationFailure, "SRV is associated with a non-existent buffer. (%cs)", context);
			}
		}
		else
		{
			log(RHI_D3D12, ValidationFailure, "SRV is not associated with any buffer. (%cs)", context);
		}
	}
	else
	{
		log(RHI_D3D12, ValidationFailure, "SRV is out of range of the CBV/SRV/UAV heap! (%cs)", context);
	}
#endif
}

fn_local void d3d12_resolve_timestamp_queries(ID3D12GraphicsCommandList *list);

void rhi_begin_frame(void)
{
	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);

	d3d12_wait_for_frame(frame->fence_value);

	d3d12_descriptor_heap_flush_pending_frees(&g_rhi.cbv_srv_uav, frame->fence_value);
	d3d12_descriptor_heap_flush_pending_frees(&g_rhi.rtv, frame->fence_value);
	d3d12_descriptor_heap_flush_pending_frees(&g_rhi.dsv, frame->fence_value);
	d3d12_flush_deferred_release_queue(frame->fence_value);

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t *window = it.data;
		window->backbuffer_index = IDXGISwapChain3_GetCurrentBackBufferIndex(window->swap_chain);
	}

	d3d12_arena_reset(&frame->upload_arena);

	ID3D12CommandAllocator *allocator = frame->direct_allocator;
	ID3D12CommandAllocator_Reset(allocator);

	ID3D12GraphicsCommandList *list = frame->direct_command_list.d3d;
	ID3D12GraphicsCommandList_Reset(list, allocator, NULL);

	ID3D12GraphicsCommandList_Reset(frame->copy_command_list, frame->copy_allocator, NULL);

	ID3D12GraphicsCommandList_SetDescriptorHeaps(list, 1, &g_rhi.cbv_srv_uav.heap);
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, g_rhi.rs_bindless);
	ID3D12GraphicsCommandList_SetComputeRootSignature (list, g_rhi.rs_bindless);

	frame->direct_command_list.index_buffer        = (rhi_buffer_t){0};
	frame->direct_command_list.render_target_count = 0;
	frame->direct_command_list.current_pso         = (rhi_pso_t){0};
}

fn_local D3D12_RENDER_TARGET_BLEND_DESC to_d3d12_rt_blend_desc(const rhi_render_target_blend_t *desc)
{
	D3D12_RENDER_TARGET_BLEND_DESC result = {
		.BlendEnable           = desc->blend_enable,
		.LogicOpEnable         = desc->logic_op_enable,
		.SrcBlend              = to_d3d12_blend   (desc->src_blend),
		.DestBlend             = to_d3d12_blend   (desc->dst_blend),
		.BlendOp               = to_d3d12_blend_op(desc->blend_op),
		.SrcBlendAlpha         = to_d3d12_blend   (desc->src_blend_alpha),
		.DestBlendAlpha        = to_d3d12_blend   (desc->dst_blend_alpha),
		.BlendOpAlpha          = to_d3d12_blend_op(desc->blend_op_alpha),
		.LogicOp               = to_d3d12_logic_op(desc->logic_op),
		.RenderTargetWriteMask = desc->write_mask,
	};

	return result;
}

fn_local D3D12_DEPTH_STENCILOP_DESC to_d3d12_depth_stencil_op_desc(const rhi_depth_stencil_op_desc_t *desc)
{
	D3D12_DEPTH_STENCILOP_DESC result = {
		.StencilFailOp      = to_d3d12_stencil_op     (desc->stencil_fail_op),
		.StencilDepthFailOp = to_d3d12_stencil_op     (desc->stencil_depth_fail_op),
		.StencilPassOp      = to_d3d12_stencil_op     (desc->stencil_pass_op),
		.StencilFunc        = to_d3d12_comparison_func(desc->stencil_func),
	};

	return result;
}

rhi_pso_t rhi_create_graphics_pso(const rhi_create_graphics_pso_params_t *params)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		.pRootSignature = g_rhi.rs_bindless,
		.VS = {
			.pShaderBytecode = params->vs.bytes,
			.BytecodeLength  = (uint32_t)params->vs.count,
		},
		.PS = {
			.pShaderBytecode = params->ps.bytes,
			.BytecodeLength  = (uint32_t)params->ps.count,
		},
		.BlendState = {
			.AlphaToCoverageEnable  = params->blend.alpha_to_coverage_enable,
			.IndependentBlendEnable = params->blend.independent_blend_enable,
			.RenderTarget[0] = to_d3d12_rt_blend_desc(&params->blend.render_target[0]),
			.RenderTarget[1] = to_d3d12_rt_blend_desc(&params->blend.render_target[1]),
			.RenderTarget[2] = to_d3d12_rt_blend_desc(&params->blend.render_target[2]),
			.RenderTarget[3] = to_d3d12_rt_blend_desc(&params->blend.render_target[3]),
			.RenderTarget[4] = to_d3d12_rt_blend_desc(&params->blend.render_target[4]),
			.RenderTarget[5] = to_d3d12_rt_blend_desc(&params->blend.render_target[5]),
			.RenderTarget[6] = to_d3d12_rt_blend_desc(&params->blend.render_target[6]),
			.RenderTarget[7] = to_d3d12_rt_blend_desc(&params->blend.render_target[7]),
		},
		.SampleMask = params->blend.sample_mask,
		.RasterizerState = {
			.FillMode              = to_d3d12_fill_mode(params->rasterizer.fill_mode),
			.CullMode              = to_d3d12_cull_mode(params->rasterizer.cull_mode),
			.FrontCounterClockwise = (params->rasterizer.front_winding == RhiWinding_ccw),
			.DepthClipEnable       = params->rasterizer.depth_clip_enable,
			.MultisampleEnable     = params->rasterizer.multisample_enable,
			.AntialiasedLineEnable = params->rasterizer.anti_aliased_line_enable,
			.ConservativeRaster    = params->rasterizer.conservative_rasterization,
		},
		.DepthStencilState = {
			.DepthEnable      = params->depth_stencil.depth_test_enable,
			.DepthWriteMask   = params->depth_stencil.depth_write,
			.DepthFunc        = to_d3d12_comparison_func(params->depth_stencil.depth_func),
			.StencilEnable    = params->depth_stencil.stencil_test_enable,
			.StencilReadMask  = params->depth_stencil.stencil_read_mask,
			.StencilWriteMask = params->depth_stencil.stencil_write_mask,
			.FrontFace        = to_d3d12_depth_stencil_op_desc(&params->depth_stencil.front),
			.BackFace         = to_d3d12_depth_stencil_op_desc(&params->depth_stencil.front),
		},
		.PrimitiveTopologyType = to_d3d12_primitive_topology_type(params->primitive_topology_type),
		.NumRenderTargets = params->render_target_count,
		.RTVFormats[0] = to_dxgi_format(params->rtv_formats[0]),
		.RTVFormats[1] = to_dxgi_format(params->rtv_formats[1]),
		.RTVFormats[2] = to_dxgi_format(params->rtv_formats[2]),
		.RTVFormats[3] = to_dxgi_format(params->rtv_formats[3]),
		.RTVFormats[4] = to_dxgi_format(params->rtv_formats[4]),
		.RTVFormats[5] = to_dxgi_format(params->rtv_formats[5]),
		.RTVFormats[6] = to_dxgi_format(params->rtv_formats[6]),
		.RTVFormats[7] = to_dxgi_format(params->rtv_formats[7]),
		.DSVFormat = to_dxgi_format(params->dsv_format),
		.SampleDesc = { .Count = MAX(1, params->multisample_count), .Quality = params->multisample_quality },
	};

	ID3D12PipelineState *d3d_pso;
	HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(g_rhi.device,
														  &pso_desc,
														  &IID_ID3D12PipelineState,
														  &d3d_pso);

	rhi_pso_t result = {0};

	if (SUCCEEDED(hr))
	{
		d3d12_pso_t *pso = pool_add(&g_rhi.psos);
		pso->kind = D3d12PsoKind_graphics;
		pso->d3d  = d3d_pso;

		DEBUG_ASSERT_MSG(params->debug_name.count, "Please assign a debug name to your PSO");
		d3d12_set_debug_name((ID3D12Object *)pso->d3d, params->debug_name);

		result = CAST_HANDLE(rhi_pso_t, pool_get_handle(&g_rhi.psos, pso));
	}
	else
	{
		log(RHI_D3D12, Error, "Failed to create PSO (TODO: Further details)");
	}

	return result;
}

rhi_pso_t rhi_create_compute_pso(const rhi_compute_pso_params_t *params)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {
		.pRootSignature = g_rhi.rs_bindless,
		.CS = {
			.pShaderBytecode = params->cs.bytes,
			.BytecodeLength  = (uint32_t)params->cs.count,
		},
	};

	ID3D12PipelineState *d3d_pso;
	HRESULT hr = ID3D12Device_CreateComputePipelineState(
		g_rhi.device,
		&pso_desc,
		&IID_ID3D12PipelineState,
		&d3d_pso);

	rhi_pso_t result = {0};

	if (SUCCEEDED(hr))
	{
		d3d12_pso_t *pso = pool_add(&g_rhi.psos);
		pso->kind = D3d12PsoKind_compute;
		pso->d3d  = d3d_pso;

		DEBUG_ASSERT_MSG(params->debug_name.count, "Please assign a debug name to your PSO");
		d3d12_set_debug_name((ID3D12Object *)pso->d3d, params->debug_name);

		result = CAST_HANDLE(rhi_pso_t, pool_get_handle(&g_rhi.psos, pso));
	}

	return result;
}

void rhi_destroy_pso(rhi_pso_t handle)
{
	// TODO: deferred release (or validation on not destroying in-use PSOs, probably if you're doing that you wanted to flush the frame)

	if (RESOURCE_HANDLE_VALID(handle))
	{
		d3d12_pso_t *pso = pool_get(&g_rhi.psos, handle);
		if (ALWAYS(pso))
		{
			COM_SAFE_RELEASE(pso->d3d);
		}

		pool_rem_item(&g_rhi.psos, pso);
	}
}

rhi_command_list_t *rhi_get_command_list(void)
{
	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);
	return &frame->direct_command_list;
}

void rhi_graphics_pass_begin(rhi_command_list_t *list, const rhi_graphics_pass_params_t *params)
{
	ASSERT(!list->began_graphics_pass);
	list->began_graphics_pass = true;

	const rhi_graphics_pass_color_attachment_t *attachments[8];

	list->render_target_count = 0;
	for (size_t i = 0; i < RhiMaxRenderTargetCount; i++)
	{
		rhi_texture_t texture_handle = params->render_targets[i].texture;

		if (RESOURCE_HANDLE_VALID(texture_handle))
		{
			uint32_t index = list->render_target_count++;

			list->render_targets[index] = texture_handle;
			attachments         [index] = &params->render_targets[i];
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rt_descriptors[8];
	D3D12_CPU_DESCRIPTOR_HANDLE *dsv = NULL;

	uint32_t barrier_count = 0;
	D3D12_RESOURCE_BARRIER barriers[9];

	for (size_t i = 0; i < list->render_target_count; i++)
	{
		const rhi_graphics_pass_color_attachment_t *attachment = &params->render_targets[i];

		rhi_texture_t    texture_handle = attachment->texture;
		d3d12_texture_t *texture        = pool_get(&g_rhi.textures, texture_handle);

		ASSERT(texture);
		ASSERT(texture->desc.usage_flags & RhiTextureUsage_render_target);

		rt_descriptors[i] = texture->rtv.cpu;

		barrier_count += d3d12_transition_state(texture->resource, &texture->state, 
												D3D12_RESOURCE_STATE_RENDER_TARGET, &barriers[i]);
	}

	if (RESOURCE_HANDLE_VALID(params->depth_stencil.texture))
	{
		d3d12_texture_t *texture = pool_get(&g_rhi.textures, params->depth_stencil.texture);
		ASSERT(texture);

		dsv = &texture->dsv.cpu;

		barrier_count += d3d12_transition_state(texture->resource, &texture->state,
												D3D12_RESOURCE_STATE_DEPTH_WRITE, &barriers[barrier_count]);
	}

	if (barrier_count > 0)
	{
		ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, barrier_count, barriers);
	}

	ID3D12GraphicsCommandList_OMSetRenderTargets(list->d3d, list->render_target_count, rt_descriptors, FALSE, dsv);

	for (size_t i = 0; i < list->render_target_count; i++)
	{
		if (attachments[i]->op == RhiPassOp_clear)
		{
			v4_t clear_color = attachments[i]->clear_color;
			ID3D12GraphicsCommandList_ClearRenderTargetView(list->d3d, rt_descriptors[i], &clear_color.e[0], 0, NULL);
		}
	}

	if (dsv && params->depth_stencil.op == RhiPassOp_clear)
	{
		ID3D12GraphicsCommandList_ClearDepthStencilView(list->d3d, *dsv, D3D12_CLEAR_FLAG_DEPTH, params->depth_stencil.clear_value, 0, 0, NULL);
	}

	ID3D12GraphicsCommandList_IASetPrimitiveTopology(list->d3d, to_d3d12_primitive_topology(params->topology));

	D3D12_VIEWPORT viewport = {
		.TopLeftX = params->viewport.min_x,
		.TopLeftY = params->viewport.min_y,
		.Width    = params->viewport.width,
		.Height   = params->viewport.height,
		.MinDepth = params->viewport.min_depth,
		.MaxDepth = params->viewport.max_depth,
	};
	ID3D12GraphicsCommandList_RSSetViewports(list->d3d, 1, &viewport);

	D3D12_RECT scissor_rect = {
		.left   = params->scissor_rect.min.x,
		.top    = params->scissor_rect.min.y,
		.right  = params->scissor_rect.max.x,
		.bottom = params->scissor_rect.max.y,
	};
	ID3D12GraphicsCommandList_RSSetScissorRects(list->d3d, 1, &scissor_rect);
}

void rhi_graphics_pass_end(rhi_command_list_t *list)
{
	ASSERT(list->began_graphics_pass);
	list->began_graphics_pass = false;

	uint32_t barrier_count = 0;
	D3D12_RESOURCE_BARRIER barriers[8];

	for (size_t i = 0; i < list->render_target_count; i++)
	{
		rhi_texture_t    texture_handle = list->render_targets[i];
		d3d12_texture_t *texture        = pool_get(&g_rhi.textures, texture_handle);

		ASSERT(texture);
		ASSERT(texture->desc.usage_flags & RhiTextureUsage_render_target);

		barrier_count += d3d12_transition_state(texture->resource, &texture->state, 
												D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, &barriers[i]);
	}

	if (RESOURCE_HANDLE_VALID(list->ds))
	{
		d3d12_texture_t *texture = pool_get(&g_rhi.textures, list->ds);
		ASSERT(texture);

		if (!(texture->desc.usage_flags & RhiTextureUsage_deny_srv))
		{
			barrier_count += d3d12_transition_state(texture->resource, &texture->state,
													D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, &barriers[barrier_count]);
		}
	}

	NULLIFY_HANDLE(&list->ds);

	if (barrier_count > 0)
	{
		ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, barrier_count, barriers);
	}
}

void rhi_set_pso(rhi_command_list_t *list, rhi_pso_t pso_handle)
{
	if (!RESOURCE_HANDLES_EQUAL(list->current_pso, pso_handle))
	{
		d3d12_pso_t *pso = pool_get(&g_rhi.psos, pso_handle);

		if (pso)
		{
			ID3D12GraphicsCommandList_SetPipelineState(list->d3d, pso->d3d);
		}
		else
		{
			log(RHI_D3D12, Error, "Tried to assign invalid PSO");
		}

		list->current_pso = pso_handle;
	}
}

void rhi_compute_pass_begin(rhi_command_list_t *list, const rhi_compute_pass_params_t *params)
{
	// TODO: This is not a very efficient use of barriers

	if (list->began_compute_pass)
	{
		log(RHI_D3D12, Error, "Began a compute pass without ending the previous one.");

		rhi_compute_pass_end(list);
	}

	m_scoped_temp
	{
		uint32_t barrier_count = 0;

		D3D12_RESOURCE_BARRIER *barriers = m_alloc_array_nozero(
			temp, 
			params->buffer_uavs_count + params->texture_uavs_count,
			D3D12_RESOURCE_BARRIER);

		// Hate this duplication
		for (size_t i = 0; i < params->buffer_uavs_count; i++)
		{
			d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, params->buffer_uavs[i]);

			uint32_t resource_index = d3d12_resource_index(buffer->flags);

			barrier_count += d3d12_transition_state(
				buffer->resources[resource_index],
				&buffer->state[resource_index],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				&barriers[barrier_count]);
		}

		// Hate this duplication
		for (size_t i = 0; i < params->texture_uavs_count; i++)
		{
			d3d12_texture_t *texture = pool_get(&g_rhi.textures, params->texture_uavs[i]);

			barrier_count += d3d12_transition_state(
				texture->resource,
				&texture->state,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				&barriers[barrier_count]);
		}

		if (barrier_count > 0)
		{
			ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, barrier_count, barriers);
		}
	}

	list->began_compute_pass = true;
	list->compute.buffer_uavs_count  = params->buffer_uavs_count;
	list->compute.buffer_uavs        = params->buffer_uavs;  // @RhiLifetimes
	list->compute.texture_uavs_count = params->texture_uavs_count;
	list->compute.texture_uavs       = params->texture_uavs; // @RhiLifetimes
}

fn void rhi_compute_pass_end(rhi_command_list_t *list)
{
	// TODO: This is not a very efficient use of barriers

	if (!list->began_compute_pass)
	{
		log(RHI_D3D12, Error, "Tried to end a compute pass without starting one the previous one.");
		return;
	}

	m_scoped_temp
	{
		uint32_t barrier_count = 0;

		D3D12_RESOURCE_BARRIER *barriers = m_alloc_array_nozero(
			temp, 
			2 * (list->compute.buffer_uavs_count + list->compute.texture_uavs_count),
			D3D12_RESOURCE_BARRIER);

		// Hate this duplication
		for (size_t i = 0; i < list->compute.buffer_uavs_count; i++)
		{
			d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, list->compute.buffer_uavs[i]);

			uint32_t resource_index = d3d12_resource_index(buffer->flags);

			barrier_count += d3d12_transition_state(
				buffer->resources[resource_index],
				&buffer->state[resource_index],
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
				&barriers[barrier_count]);
		}

		// Hate this duplication
		for (size_t i = 0; i < list->compute.texture_uavs_count; i++)
		{
			d3d12_texture_t *texture = pool_get(&g_rhi.textures, list->compute.texture_uavs[i]);

			barrier_count += d3d12_transition_state(
				texture->resource,
				&texture->state,
				D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
				&barriers[barrier_count]);
		}

		if (barrier_count > 0)
		{
			ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, barrier_count, barriers);
		}
	}

	list->began_compute_pass = false;
	zero_struct(&list->compute);
}

void rhi_set_parameters(rhi_command_list_t *list, uint32_t slot, void *parameters, uint32_t size)
{
	ASSERT(slot <= 3);

	if (slot == 0)
	{
		if (NEVER(size % sizeof(uint32_t) != 0))
		{
			log(RHI_D3D12, Error, "called rhi_set_parameters on slot 0 with a parameter size that is not a multiple of 4, which is not allowed");
			return;
		}

		const uint32_t constants_count = size / sizeof(uint32_t);

		// TODO: it's silly to always do both but right now I don't want to bifurcate the calls
		ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(
			list->d3d, 
			D3d12BindlessRootParameter_32bitconstants, 
			constants_count, 
			parameters, 
			0);

		ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(
			list->d3d, 
			D3d12BindlessRootParameter_32bitconstants, 
			constants_count, 
			parameters, 
			0);
	}
	else
	{
		d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);

		d3d12_buffer_allocation_t alloc = d3d12_arena_alloc(&frame->upload_arena, size, 256);
		memcpy(alloc.cpu, parameters, size);

		// TODO: it's silly to always do both but right now I don't want to bifurcate the calls
		ID3D12GraphicsCommandList_SetComputeRootConstantBufferView(list->d3d, slot, alloc.gpu);
		ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(list->d3d, slot, alloc.gpu);
	}
}

rhi_parameters_t rhi_alloc_parameters(uint32_t size)
{
	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);
	d3d12_buffer_allocation_t alloc = d3d12_arena_alloc(&frame->upload_arena, size, 256);

	rhi_parameters_t result = {
		.host   = alloc.cpu,
		.offset = (uint32_t)alloc.offset,
		.size   = size,
	};

	return result;
}

void rhi_draw(rhi_command_list_t *list, uint32_t vertex_count, uint32_t start_vertex)
{
	DEBUG_ASSERT(list);

#if DREAM_SLOW
	d3d12_pso_t *pso = pool_get(&g_rhi.psos, list->current_pso);
	ASSERT_MSG(pso, "Called rhi_dispatch without setting a PSO");
	ASSERT_MSG(pso->kind == D3d12PsoKind_graphics, "called rhi_dispatch, but the current PSO is a compute PSO!");
#endif

	ID3D12GraphicsCommandList_DrawInstanced(list->d3d, vertex_count, 1, start_vertex, 0);
}

void rhi_draw_indexed(rhi_command_list_t *list, rhi_buffer_t index_buffer_handle, uint32_t index_count, uint32_t start_index, uint32_t start_vertex)
{
	DEBUG_ASSERT(list);

#if DREAM_SLOW
	d3d12_pso_t *pso = pool_get(&g_rhi.psos, list->current_pso);
	ASSERT_MSG(pso, "Called rhi_dispatch without setting a PSO");
	ASSERT_MSG(pso->kind == D3d12PsoKind_graphics, "called rhi_dispatch, but the current PSO is a compute PSO!");
#endif

	if (!RESOURCE_HANDLES_EQUAL(list->index_buffer, index_buffer_handle))
	{
		if (RESOURCE_HANDLE_VALID(index_buffer_handle))
		{
			d3d12_buffer_t *index_buffer = pool_get(&g_rhi.buffers, index_buffer_handle);
			ASSERT(index_buffer);

			uint32_t resource_index = d3d12_resource_index(index_buffer->flags);

			D3D12_INDEX_BUFFER_VIEW ibv = {
				.BufferLocation = index_buffer->gpu_address[resource_index],
				.SizeInBytes    = index_buffer->size,
				.Format         = DXGI_FORMAT_R16_UINT, // TODO: Unhardcode
			};

			ID3D12GraphicsCommandList_IASetIndexBuffer(list->d3d, &ibv);
		}
		else
		{
			ID3D12GraphicsCommandList_IASetIndexBuffer(list->d3d, NULL);
		}

		list->index_buffer = index_buffer_handle;
	}

	ID3D12GraphicsCommandList_DrawIndexedInstanced(list->d3d, index_count, 1, start_index, start_vertex, 0);
}

void rhi_draw_instanced(rhi_command_list_t *list, uint32_t vertex_count, uint32_t start_vertex, uint32_t instance_count, uint32_t start_instance)
{
	DEBUG_ASSERT(list);

	ID3D12GraphicsCommandList_DrawInstanced(list->d3d, vertex_count, instance_count, start_vertex, start_instance);
}

void rhi_dispatch(rhi_command_list_t *list, int dispatch_x, int dispatch_y, int dispatch_z)
{
	ASSERT_MSG(list->began_compute_pass, "You need to begin a comput pass before calling rhi_dispatch");

#if DREAM_SLOW
	d3d12_pso_t *pso = pool_get(&g_rhi.psos, list->current_pso);
	ASSERT_MSG(pso, "Called rhi_dispatch without setting a PSO");
	ASSERT_MSG(pso->kind == D3d12PsoKind_compute, "called rhi_dispatch, but the current PSO is a graphics PSO!");
#endif

	ID3D12GraphicsCommandList_Dispatch(list->d3d, dispatch_x, dispatch_y, dispatch_z);

	// sketchy idea:

	m_scoped_temp
	{
		uint32_t barrier_count = 0;

		D3D12_RESOURCE_BARRIER *barriers = m_alloc_array_nozero(
			temp, 
			list->compute.buffer_uavs_count + list->compute.texture_uavs_count,
			D3D12_RESOURCE_BARRIER);

		// Hate this duplication
		for (size_t i = 0; i < list->compute.buffer_uavs_count; i++)
		{
			d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, list->compute.buffer_uavs[i]);

			uint32_t resource_index = d3d12_resource_index(buffer->flags);
			d3d12_uav_barrier(buffer->resources[resource_index], &barriers[barrier_count++]);
		}

		// Hate this duplication
		for (size_t i = 0; i < list->compute.texture_uavs_count; i++)
		{
			d3d12_texture_t *texture = pool_get(&g_rhi.textures, list->compute.texture_uavs[i]);
			d3d12_uav_barrier(texture->resource, &barriers[barrier_count++]);
		}

		if (barrier_count > 0)
		{
			ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, barrier_count, barriers);
		}
	}
}

void rhi_end_frame(void)
{
	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);

	// submit frame copies
	{
		ID3D12CommandQueue_BeginEvent(g_rhi.copy_queue, 1, "Copy Frame Resources", sizeof("Copy Frame Resources"));

		ID3D12GraphicsCommandList *copy_list = frame->copy_command_list;
		ID3D12GraphicsCommandList_Close(copy_list);

		ID3D12CommandList *lists[] = { (ID3D12CommandList *)copy_list };
		ID3D12CommandQueue_ExecuteCommandLists(g_rhi.copy_queue, 1, lists);

		uint64_t copy_fence_value = ++g_rhi.copy_fence_value;
		ID3D12CommandQueue_Signal(g_rhi.copy_queue, g_rhi.copy_fence, copy_fence_value);

		ID3D12CommandQueue_EndEvent(g_rhi.copy_queue);

		// synchronize direct queue
		ID3D12CommandQueue_Wait(g_rhi.direct_queue, g_rhi.copy_fence, copy_fence_value);
	}

	ID3D12GraphicsCommandList *list = frame->direct_command_list.d3d;

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t  *window     = it.data;
		d3d12_texture_t *backbuffer = pool_get(&g_rhi.textures, window->frame_buffers[window->backbuffer_index]);

		D3D12_RESOURCE_BARRIER barrier;
		if (d3d12_transition_state(backbuffer->resource, &backbuffer->state, D3D12_RESOURCE_STATE_PRESENT, &barrier))
		{
			ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
		}
	}

	if (g_rhi.timestamps_readback && cvar_read_bool(&cvar_d3d12_timestamp_queries_enabled))
	{
		d3d12_resolve_timestamp_queries(list);
	}

	ID3D12GraphicsCommandList_Close(list);

	if (cvar_read_bool(&cvar_d3d12_force_heavy_synchronization))
	{
		ID3D12CommandQueue_Wait(g_rhi.direct_queue, g_rhi.fence, g_rhi.fence_value);
	}

	ID3D12CommandQueue_BeginEvent(g_rhi.direct_queue, 1, "Draw Frame", sizeof("Draw Frame"));

	ID3D12CommandList *lists[] = { (ID3D12CommandList *)list };
	ID3D12CommandQueue_ExecuteCommandLists(g_rhi.direct_queue, 1, lists);

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t *window = it.data;

		DXGI_PRESENT_PARAMETERS present_parameters = { 0 };
		IDXGISwapChain1_Present1(window->swap_chain, 1, 0, &present_parameters);
	}

	frame->fence_value = ++g_rhi.fence_value;

	HRESULT hr = ID3D12CommandQueue_Signal(g_rhi.direct_queue, g_rhi.fence, frame->fence_value);
	D3D12_CHECK_HR(hr, return);

	ID3D12CommandQueue_EndEvent(g_rhi.direct_queue);

	g_rhi.frame_index += 1;
}

rhi_shader_bytecode_t rhi_compile_shader(arena_t *arena, string_t shader_source, string_t file_name, string_t entry_point, string_t shader_model)
{
	rhi_shader_bytecode_t result = {0};

	arena_t *temp = m_get_temp_scope_begin(&arena, 1);
	{
		wchar_t *args[] = { 
			L"-E", utf16_from_utf8(temp, entry_point).data, 
			L"-T", utf16_from_utf8(temp, shader_model).data, 
			L"-I", L"../src/shaders",
			L"-WX", 
			L"-Zi", 
			// L"-no-legacy-cbuf-layout", can't get this to work, seemingly
		};

		HRESULT hr;

		ID3DBlob *error    = NULL;
		ID3DBlob *bytecode = NULL;
		hr = dxc_compile(shader_source.data, (uint32_t)shader_source.count, args, ARRAY_COUNT(args), &bytecode, &error);

		if (error && ID3D10Blob_GetBufferSize(error) > 0)
		{
			const char *message = ID3D10Blob_GetBufferPointer(error);

			bool is_fatal = FAILED(hr); 
			string_t formatted_message = string_format(temp, "Shader compilation %s for '%.*s:%.*s' (%.*s):\n%s", 
													   is_fatal ? "error" : "warning", 
													   Sx(file_name), 
													   Sx(entry_point), 
													   Sx(shader_model), 
													   message);

			// A downside of these log macros is that the category and level can't be changed at runtime
			if (is_fatal)
			{
				logs(RHI_D3D12, Error, formatted_message);
			}
			else
			{
				logs(RHI_D3D12, Warning, formatted_message);
			}

			if (is_fatal)
			{
				FATAL_ERROR("%.*s", Sx(formatted_message));
			}

			COM_SAFE_RELEASE(error);
		}
		
		if (bytecode)
		{
			void     *bytecode_bytes = ID3D10Blob_GetBufferPointer(bytecode);
			uint32_t  bytecode_size  = ID3D10Blob_GetBufferSize(bytecode);

			result.bytes = m_copy(arena, bytecode_bytes, bytecode_size);
			result.count = bytecode_size;

			COM_SAFE_RELEASE(bytecode);
		}
	}
	m_scope_end(temp);

	return result;
}

void d3d12_flush_direct_command_queue(void)
{
	uint64_t fence_value = ID3D12Fence_GetCompletedValue(g_rhi.fence);

	if (fence_value < g_rhi.fence_value)
	{
		ID3D12Fence_SetEventOnCompletion(g_rhi.fence, g_rhi.fence_value, NULL);
	}
}

void rhi_begin_region(rhi_command_list_t *list, string_t region)
{
	m_scoped_temp
	{
		string_t terminated = string_null_terminate(temp, region);
		ID3D12GraphicsCommandList_BeginEvent(list->d3d, 1, terminated.data, (UINT)terminated.count + 1);
	}
}

void rhi_end_region(rhi_command_list_t *list)
{
	ID3D12GraphicsCommandList_EndEvent(list->d3d);
}

void rhi_init_timestamps(uint32_t max_timestamps_per_frame_count)
{
	ASSERT_MSG(g_rhi.max_timestamps_per_frame == 0, "You can't init timestamps more than once!");

	uint32_t capacity = g_rhi.frame_latency * max_timestamps_per_frame_count; // TODO: What if we switch between double and triple buffering at runtime?

	HRESULT hr;

	{
		D3D12_QUERY_HEAP_DESC desc = {
			.Type  = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
			.Count = capacity,
		};

		hr = ID3D12Device_CreateQueryHeap(g_rhi.device, &desc, &IID_ID3D12QueryHeap, &g_rhi.timestamps_heap);
		ASSERT(SUCCEEDED(hr));

		d3d12_set_debug_name((ID3D12Object *)g_rhi.timestamps_heap, S("Timestamps Query Heap"));
	}

	uint32_t readback_size = sizeof(uint64_t)*capacity;
	g_rhi.timestamps_readback = d3d12_create_readback_buffer(g_rhi.device, readback_size, S("Timestamp Query Readback Buffer"));

	g_rhi.max_timestamps_per_frame = max_timestamps_per_frame_count;
}

void rhi_record_timestamp(rhi_command_list_t *list, uint32_t index)
{
	if (!cvar_read_bool(&cvar_d3d12_timestamp_queries_enabled))
	{
		return;
	}

	ASSERT_MSG(index < g_rhi.max_timestamps_per_frame, 
			   "Timestamp index out of bounds! max: %u, index: %u", g_rhi.max_timestamps_per_frame, index);

	g_rhi.timestamps_watermark = MAX(g_rhi.timestamps_watermark, index);

	const uint32_t frame_index  = g_rhi.frame_index % g_rhi.frame_latency;
	const uint32_t actual_index = frame_index*g_rhi.max_timestamps_per_frame + index;
	ID3D12GraphicsCommandList_EndQuery(list->d3d, g_rhi.timestamps_heap, D3D12_QUERY_TYPE_TIMESTAMP, actual_index);
}

uint64_t rhi_get_timestamp_frequency(void)
{
	uint64_t result = 0;
	ID3D12CommandQueue_GetTimestampFrequency(g_rhi.direct_queue, &result);

	return result;
}

uint64_t *rhi_begin_read_timestamps(void)
{
	ASSERT_MSG(!g_rhi.timestamps_readback_is_mapped, "You can't begin reading timestamps without first ending the previous read");
	ASSERT_MSG(g_rhi.timestamps_readback, "You have to initialize timestamps before you can try to read them");

	const uint32_t frame_index            = g_rhi.frame_index % g_rhi.frame_latency;
	const uint32_t timestamps_start_index = frame_index*g_rhi.max_timestamps_per_frame;
	
	void *mapped = NULL;

	D3D12_RANGE read_range = {
		.Begin = sizeof(uint64_t)*(timestamps_start_index),
		.End   = sizeof(uint64_t)*(timestamps_start_index + g_rhi.max_timestamps_per_frame),
	};

	HRESULT hr = ID3D12Resource_Map(g_rhi.timestamps_readback, 0, &read_range, &mapped);
	D3D12_CHECK_HR(hr, return NULL);

	g_rhi.timestamps_readback_is_mapped = true;

	return mapped;
}

void rhi_end_read_timestamps(void)
{
	ASSERT_MSG(g_rhi.timestamps_readback_is_mapped, "You can't end reading timestamps without first beginning a read");

	ID3D12Resource_Unmap(g_rhi.timestamps_readback, 0, &(D3D12_RANGE){0});

	g_rhi.timestamps_readback_is_mapped = false;
}

void d3d12_resolve_timestamp_queries(ID3D12GraphicsCommandList *list)
{
	const uint32_t frame_index = g_rhi.frame_index % g_rhi.frame_latency;
	const uint32_t start_index = frame_index*g_rhi.max_timestamps_per_frame;

	// TODO: This doesn't work. Need to handle sparse queries
	const uint32_t queries_to_resolve = g_rhi.timestamps_watermark + 1;

	ID3D12GraphicsCommandList_ResolveQueryData(list, 
											   g_rhi.timestamps_heap,
											   D3D12_QUERY_TYPE_TIMESTAMP,
											   start_index,
											   queries_to_resolve,
											   g_rhi.timestamps_readback,
											   sizeof(uint64_t)*start_index);

	g_rhi.timestamps_watermark = 0;
}

void rhi_marker(rhi_command_list_t *list, string_t marker)
{
	m_scoped_temp
	{
		string_t terminated = string_null_terminate(temp, marker);
		ID3D12GraphicsCommandList_SetMarker(list->d3d, 1, terminated.data, (UINT)terminated.count + 1);
	}
}

void rhi_flush_everything(void)
{
	d3d12_flush_direct_command_queue();
	d3d12_flush_ring_buffer_uploads();
}

global bool initialized_test_stuff;
global rhi_buffer_t args_buffer;

void rhi_do_test_stuff(rhi_command_list_t *list, rhi_texture_t hdr_color, rhi_texture_t rt, rhi_pso_t pso)
{
	if (!initialized_test_stuff)
	{
		args_buffer = rhi_create_buffer(&(rhi_create_buffer_params_t){
			.debug_name = S("test_indirect_args"),
			.desc = {
				.first_element  = 0,
				.element_count  = 4096,
				.element_stride = sizeof(rhi_indirect_draw_t),
			},
			.heap  = RhiHeapKind_upload,
			.flags = RhiResourceFlag_dynamic,
		});

		d3d12_buffer_t *args = pool_get(&g_rhi.buffers, args_buffer);
		for (size_t i = 0; i < g_rhi.frame_latency; i++)
		{
			HRESULT hr = ID3D12Resource_Map(args->resources[i], 0, &(D3D12_RANGE){ 0 }, &args->mapped[i]);
			ASSERT(SUCCEEDED(hr));
		}

		initialized_test_stuff = true;
	}

	d3d12_buffer_t *buffer_d3d = pool_get(&g_rhi.buffers, args_buffer);
	uint32_t resource_index = d3d12_resource_index(buffer_d3d->flags);

	rhi_indirect_draw_t *args = buffer_d3d->mapped[resource_index];

	args->vertex_count   = 3;
	args->instance_count = 1;

	rhi_simple_graphics_pass_begin(list, rt, (rhi_texture_t){0}, RhiPrimitiveTopology_trianglelist);
	{
		rhi_set_pso(list, pso);

		post_draw_parameters_t draw_parameters = {
			.hdr_color    = rhi_get_texture_srv(hdr_color),
			.sample_count = r1->multisample_count,
		};
		shader_post_set_draw_params(list, &draw_parameters);

		ID3D12GraphicsCommandList_ExecuteIndirect(
			list->d3d,
			g_rhi.draw_command_signature,
			1,
			buffer_d3d->resources[resource_index],
			0,
			NULL,
			0);
	}
	rhi_graphics_pass_end(list);
}
