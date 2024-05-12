// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#include "dxc.h"

#include "d3d12_helpers.c"
#include "d3d12_buffer_arena.c"
#include "d3d12_descriptor_heap.c" 
#include "d3d12_constants.c"
#include "d3d12_upload.c"

fn_local d3d12_frame_state_t *d3d12_get_frame_state(uint32_t frame_index)
{
	return g_rhi.frames[frame_index % g_rhi.frame_latency];
}

fn_local void d3d12_wait_for_frame(uint32_t fence_value)
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
									 D3D12_RESOURCE_BARRIER* barrier)
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

bool rhi_init_d3d12(const rhi_init_params_d3d12_t *params)
{
	g_rhi.windows  = (pool_t)INIT_POOL_EX(d3d12_window_t,  POOL_FLAGS_CONCURRENT);
	g_rhi.buffers  = (pool_t)INIT_POOL_EX(d3d12_buffer_t,  POOL_FLAGS_CONCURRENT);
	g_rhi.textures = (pool_t)INIT_POOL_EX(d3d12_texture_t, POOL_FLAGS_CONCURRENT);
	g_rhi.psos     = (pool_t)INIT_POOL_EX(d3d12_pso_t,     POOL_FLAGS_CONCURRENT);

	//
	//
	//

	HRESULT hr = 0;

	IDXGIFactory6       *dxgi_factory         = NULL;
	IDXGIAdapter1       *dxgi_adapter         = NULL;
	ID3D12Device        *device               = NULL;
	ID3D12CommandQueue  *direct_command_queue = NULL;
	ID3D12CommandQueue  *copy_command_queue   = NULL;
	ID3D12Fence         *fence                = NULL;
	ID3D12RootSignature *root_signature       = NULL;

	bool result = false;

	//
	// Validate Parameters
	//

	if (params->base.frame_latency == 0 ||
		params->base.frame_latency >  RhiMaxFrameLatency)
	{
		log(RHI_D3D12, Error, "Invalid frame latency %u (should between 1 and 3)", params->base.frame_latency);
		goto bail;
	}

	g_rhi.frame_latency = params->base.frame_latency;

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
	// Create the direct command queue
	//

	{
		D3D12_COMMAND_QUEUE_DESC desc = {
			.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		};

		hr = ID3D12Device_CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue, &direct_command_queue);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	// Create the copy command queue
	//

	{
		D3D12_COMMAND_QUEUE_DESC desc = {
			.Type     = D3D12_COMMAND_LIST_TYPE_COPY,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		};

		hr = ID3D12Device_CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue, &copy_command_queue);
		D3D12_CHECK_HR(hr, goto bail);
	}

	//
	// Create the copy command list
	//

	//
	// Create per-frame state
	//

	for (size_t i = 0; i < g_rhi.frame_latency; i++)
	{
		d3d12_frame_state_t *frame = m_alloc_struct(&g_rhi.arena, d3d12_frame_state_t);

		{
			hr = ID3D12Device_CreateCommandAllocator(device, 
													 D3D12_COMMAND_LIST_TYPE_DIRECT, 
													 &IID_ID3D12CommandAllocator, 
													 &frame->direct_command_allocator);
			D3D12_CHECK_HR(hr, goto bail);

			hr = ID3D12Device_CreateCommandList(device,
												0,
												D3D12_COMMAND_LIST_TYPE_DIRECT,
												frame->direct_command_allocator,
												NULL,
												&IID_ID3D12GraphicsCommandList,
												&frame->direct_command_list.d3d);
			D3D12_CHECK_HR(hr, goto bail);

			ID3D12GraphicsCommandList_Close(frame->direct_command_list.d3d);

			hr = ID3D12Device_CreateCommandAllocator(device, 
													 D3D12_COMMAND_LIST_TYPE_COPY, 
													 &IID_ID3D12CommandAllocator, 
													 &frame->copy_command_allocator);
			D3D12_CHECK_HR(hr, goto bail);

			hr = ID3D12Device_CreateCommandList(device,
												0,
												D3D12_COMMAND_LIST_TYPE_COPY,
												frame->copy_command_allocator,
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
			[0] = {
				.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
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

		g_rhi.fence_value += 1;

		g_rhi.fence_event = CreateEvent(0, FALSE, FALSE, NULL);
		if (!g_rhi.fence_event)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
			D3D12_CHECK_HR(hr, goto bail);
		}
	}

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

	//
	// Successfully Initialized D3D12
	//

	g_rhi.dxgi_factory         = dxgi_factory;         dxgi_factory         = NULL;
	g_rhi.dxgi_adapter         = dxgi_adapter;         dxgi_adapter         = NULL;
	g_rhi.device               = device;               device               = NULL;
	g_rhi.direct_command_queue = direct_command_queue; direct_command_queue = NULL;
	g_rhi.copy_command_queue   = copy_command_queue;   copy_command_queue   = NULL;
	g_rhi.fence                = fence;                fence                = NULL;
	g_rhi.rs_bindless          = root_signature;       root_signature       = NULL;

	result = true;
	g_rhi.initialized = true;

	log(RHI_D3D12, Info, "Successfully initialized D3D12");

bail:
	COM_SAFE_RELEASE(dxgi_factory);
	COM_SAFE_RELEASE(dxgi_adapter);
	COM_SAFE_RELEASE(device);
	COM_SAFE_RELEASE(copy_command_queue);
	COM_SAFE_RELEASE(fence);
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

	hr = IDXGIFactory6_CreateSwapChainForHwnd(g_rhi.dxgi_factory, (IUnknown*)g_rhi.direct_command_queue, hwnd, &desc, NULL, NULL, (IDXGISwapChain1**)&swap_chain);
	D3D12_CHECK_HR(hr, goto bail);

	IDXGIFactory4_MakeWindowAssociation(g_rhi.dxgi_factory, hwnd, DXGI_MWA_NO_ALT_ENTER);

	//
	// Create RTVs
	//

	{
		for (size_t i = 0; i < g_rhi.frame_latency; i++)
		{
			d3d12_texture_t *frame_buffer = pool_add(&g_rhi.textures);
			frame_buffer->state = D3D12_RESOURCE_STATE_COMMON|D3D12_RESOURCE_STATE_PRESENT;

			hr = IDXGISwapChain1_GetBuffer(swap_chain, (uint32_t)i, &IID_ID3D12Resource, &frame_buffer->resource);
			D3D12_CHECK_HR(hr, goto bail);

			D3D12_RESOURCE_DESC rt_desc;
			ID3D12Resource_GetDesc(frame_buffer->resource, &rt_desc);

			frame_buffer->desc = rhi_texture_desc_from_d3d12_resource_desc(&rt_desc);

			frame_buffer->rtv = d3d12_allocate_descriptor_persistent(&g_rhi.rtv);
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
	ASSERT(data->subresource);
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

	uint8_t *src = data->subresource[0];
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
				.RowPitch = (uint32_t)src_stride,
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
			case RhiTextureDimension_1d: dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D; break;
			case RhiTextureDimension_2d: dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
			case RhiTextureDimension_3d: dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D; break;
			INVALID_DEFAULT_CASE;
		}

		D3D12_RESOURCE_FLAGS resource_flags = 0;
		if (params->usage & RhiTextureUsage_render_target) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (params->usage & RhiTextureUsage_depth_stencil) resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (params->usage & RhiTextureUsage_uav)           resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (params->usage & RhiTextureUsage_deny_srv)      resource_flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

		ID3D12Resource *resource = NULL;

		D3D12_RESOURCE_STATES initial_state = params->initial_data ? 
			D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

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
				.Format           = to_dxgi_format(params->format),
				.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
				.Flags            = resource_flags,
				.SampleDesc       = { .Count = 1 },
			}),
			initial_state,
			NULL, // TODO: Add to API
			&IID_ID3D12Resource,
			&resource);

		if (SUCCEEDED(hr))
		{
			d3d12_texture_t *texture = pool_add(&g_rhi.textures);
			texture->state    = initial_state;
			texture->resource = resource;

			d3d12_set_debug_name((ID3D12Object *)texture->resource, params->debug_name);

			D3D12_RESOURCE_DESC desc;
			ID3D12Resource_GetDesc(texture->resource, &desc);

			texture->desc = rhi_texture_desc_from_d3d12_resource_desc(&desc);

			if (texture->desc.usage_flags & RhiTextureUsage_render_target)
			{
				texture->rtv = d3d12_allocate_descriptor_persistent(&g_rhi.rtv);
				ID3D12Device_CreateRenderTargetView(g_rhi.device, texture->resource, NULL, texture->rtv.cpu);
			}

			if (texture->desc.usage_flags & RhiTextureUsage_depth_stencil)
			{
				texture->dsv = d3d12_allocate_descriptor_persistent(&g_rhi.dsv);
				ID3D12Device_CreateDepthStencilView(g_rhi.device, texture->resource, NULL, texture->dsv.cpu);
			}

			if (texture->desc.usage_flags & RhiTextureUsage_uav)
			{
				texture->uav = d3d12_allocate_descriptor_persistent(&g_rhi.cbv_srv_uav);
				ID3D12Device_CreateUnorderedAccessView(g_rhi.device, texture->resource, NULL, NULL, texture->uav.cpu);
			}

			if (!(texture->desc.usage_flags & RhiTextureUsage_deny_srv))
			{
				texture->srv = d3d12_allocate_descriptor_persistent(&g_rhi.cbv_srv_uav);
				ID3D12Device_CreateShaderResourceView(g_rhi.device, texture->resource, NULL, texture->srv.cpu);
			}

			rhi_texture_data_t *const initial_data = params->initial_data;

			if (initial_data)
			{
				d3d12_upload_texture_data(texture, initial_data);
			}

			result = CAST_HANDLE(rhi_texture_t, pool_get_handle(&g_rhi.textures, texture));
		}
	}

	return result;
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

fn_local void d3d12_upload_buffer_data(d3d12_buffer_t *buffer, uint64_t dst_offset, const rhi_buffer_data_t *data)
{
	m_scoped_temp
	{
		string_t debug_name = d3d12_get_debug_name(temp, (ID3D12Object *)buffer->resource);
		log(RHI_D3D12, Spam, "Uploading data for buffer '%.*s'", Sx(debug_name));
	}

	d3d12_upload_context_t ctx = d3d12_upload_begin(data->size, 256);

	{
		D3D12_RESOURCE_BARRIER barrier;

		if (d3d12_transition_state(buffer->resource, &buffer->state, D3D12_RESOURCE_STATE_COPY_DEST, &barrier))
		{
			ID3D12GraphicsCommandList_ResourceBarrier(ctx.command_list, 1, &barrier);
		}
	}

	memcpy(ctx.pointer, data->ptr, data->size);
	ID3D12GraphicsCommandList_CopyBufferRegion(ctx.command_list, 
											   buffer->resource, 
											   dst_offset, 
											   ctx.buffer, 
											   ctx.offset, 
											   data->size);

	{
		D3D12_RESOURCE_BARRIER barrier;

		ASSERT(d3d12_transition_state(buffer->resource, &buffer->state, D3D12_RESOURCE_STATE_COMMON, &barrier));
		ID3D12GraphicsCommandList_ResourceBarrier(ctx.command_list, 1, &barrier);
	}

	buffer->upload_fence_value = d3d12_upload_end(&ctx);
}

void rhi_upload_buffer_data(rhi_buffer_t handle, size_t dst_offset, const rhi_buffer_data_t *src)
{
	d3d12_buffer_t *buffer = pool_get(&g_rhi.buffers, handle);
	ASSERT_MSG(buffer, "Invalid buffer!");

	d3d12_upload_buffer_data(buffer, dst_offset, src);
}

rhi_buffer_t rhi_create_buffer(const rhi_create_buffer_params_t *params)
{
	ASSERT_MSG(params, "Called rhi_create_buffer without any parameters!");

	rhi_buffer_t result = {0};

	m_scoped_temp
	{
		D3D12_HEAP_PROPERTIES heap_properties = {
			.Type = D3D12_HEAP_TYPE_DEFAULT,
		};

		D3D12_RESOURCE_DESC desc = {
			.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Width            = params->size,
			.Height           = 1,
			.DepthOrArraySize = 1,
			.MipLevels        = 1,
			.SampleDesc       = { .Count = 1, .Quality = 0 },
			.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Format           = DXGI_FORMAT_UNKNOWN,
		};

		ID3D12Resource *resource;
		HRESULT hr = ID3D12Device_CreateCommittedResource(g_rhi.device,
														  &heap_properties,
														  D3D12_HEAP_FLAG_NONE,
														  &desc,
														  D3D12_RESOURCE_STATE_COMMON,
														  NULL,
														  &IID_ID3D12Resource,
														  &resource);
		if (SUCCEEDED(hr))
		{
			d3d12_buffer_t *buffer = pool_add(&g_rhi.buffers);
			result = CAST_HANDLE(rhi_buffer_t, pool_get_handle(&g_rhi.buffers, buffer));

			buffer->resource = resource;

			d3d12_set_debug_name((ID3D12Object *)buffer->resource, params->debug_name);

			if (params->srv)
			{
				d3d12_descriptor_t descriptor = d3d12_allocate_descriptor_persistent(&g_rhi.cbv_srv_uav);
				buffer->srv = descriptor;

				{
					D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
						.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
						.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER,
						.Buffer = {
							.FirstElement        = params->srv->first_element,
							.NumElements         = params->srv->element_count,
							.StructureByteStride = params->srv->element_stride,
							.Flags = params->srv->raw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE,
						},
					};

					ID3D12Device_CreateShaderResourceView(g_rhi.device, buffer->resource, &desc, descriptor.cpu);
				}
			}

			const rhi_buffer_data_t *data = &params->initial_data;

			if (data->ptr && ALWAYS(data->size <= params->size))
			{
				// Lying about the initial state because I can't set the initial state on the buffer without triggering a
				// debug layer warning about the initial state being ignored because it's assumed on first use... Ok... That
				// really makes it easier to do the manual state tracking as the user, for the state to just be ~whatever~ based
				// on what you do with the resource!
				buffer->state = D3D12_RESOURCE_STATE_COPY_DEST;
				d3d12_upload_buffer_data(buffer, 0, &params->initial_data);
			}
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
	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);

	d3d12_wait_for_frame(frame->fence_value);

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t *window = it.data;
		window->backbuffer_index = IDXGISwapChain3_GetCurrentBackBufferIndex(window->swap_chain);
	}

	d3d12_arena_reset(&frame->upload_arena);

	ID3D12CommandAllocator *allocator = frame->direct_command_allocator;
	ID3D12CommandAllocator_Reset(allocator);

	ID3D12GraphicsCommandList *list = frame->direct_command_list.d3d;
	ID3D12GraphicsCommandList_Reset(list, allocator, NULL);

	ID3D12GraphicsCommandList_SetDescriptorHeaps(list, 1, &g_rhi.cbv_srv_uav.heap);
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, g_rhi.rs_bindless);
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
		pso->d3d = d3d_pso;

		result = CAST_HANDLE(rhi_pso_t, pool_get_handle(&g_rhi.psos, pso));
	}
	else
	{
		log(RHI_D3D12, Error, "Failed to create PSO (TODO: Further details)");
	}

	return result;
}

rhi_command_list_t *rhi_get_command_list(void)
{
	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);
	return &frame->direct_command_list;
}

void rhi_graphics_pass_begin(rhi_command_list_t *list, const rhi_graphics_pass_params_t *params)
{
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

	if (list->render_target_count > 0)
	{
		uint32_t barrier_count = 0;
		D3D12_RESOURCE_BARRIER barriers[8];

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

		if (barrier_count > 0)
		{
			ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, barrier_count, barriers);
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE *dsv = NULL;
	if (RESOURCE_HANDLE_VALID(params->depth_stencil))
	{
		d3d12_texture_t *texture = pool_get(&g_rhi.textures, params->depth_stencil);
		ASSERT(texture);

		dsv = &texture->dsv.cpu;
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

	if (barrier_count > 0)
	{
		ID3D12GraphicsCommandList_ResourceBarrier(list->d3d, barrier_count, barriers);
	}
}

void rhi_set_pso(rhi_command_list_t *list, rhi_pso_t pso_handle)
{
	d3d12_pso_t *pso = pool_get(&g_rhi.psos, pso_handle);

	if (ALWAYS(pso))
	{
		ID3D12GraphicsCommandList_SetPipelineState(list->d3d, pso->d3d);
	}
	else
	{
		log(RHI_D3D12, Error, "Tried to assign invalid PSO");
	}
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

		ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(list->d3d, 
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

		ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(list->d3d, slot, alloc.gpu);
	}
}

void rhi_draw(rhi_command_list_t *list, uint32_t vertex_count)
{
	ID3D12GraphicsCommandList_DrawInstanced(list->d3d, vertex_count, 1, 0, 0);
}

void rhi_end_frame(void)
{
	d3d12_frame_state_t *frame = d3d12_get_frame_state(g_rhi.frame_index);

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

	ID3D12GraphicsCommandList_Close(list);

	ID3D12CommandList *lists[] = { (ID3D12CommandList *)list };
	ID3D12CommandQueue_ExecuteCommandLists(g_rhi.direct_command_queue, 1, lists);

	frame->fence_value = g_rhi.fence_value++;

	HRESULT hr = ID3D12CommandQueue_Signal(g_rhi.direct_command_queue, g_rhi.fence, frame->fence_value);
	D3D12_CHECK_HR(hr, return);

	for (pool_iter_t it = pool_iter(&g_rhi.windows);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d12_window_t *window = it.data;

		DXGI_PRESENT_PARAMETERS present_parameters = { 0 };
		IDXGISwapChain1_Present1(window->swap_chain, 1, 0, &present_parameters);
	}

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
