ID3D12Resource *d3d12_create_upload_buffer(ID3D12Device *device, uint32_t size, void *initial_data, string_t debug_name)
{
	D3D12_HEAP_PROPERTIES heap_properties = {
		.Type = D3D12_HEAP_TYPE_UPLOAD,
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
	};

	ID3D12Resource *buffer;
	HRESULT hr = ID3D12Device_CreateCommittedResource(device,
													  &heap_properties,
													  D3D12_HEAP_FLAG_NONE,
													  &desc,
													  D3D12_RESOURCE_STATE_COMMON,
													  NULL,
													  &IID_ID3D12Resource,
													  &buffer);
	D3D12_CHECK_HR(hr, return NULL);

	if (initial_data)
	{
		void *mapped = NULL;

		hr = ID3D12Resource_Map(buffer, 0, &(D3D12_RANGE){ 0 }, &mapped);
		D3D12_CHECK_HR(hr, return NULL);

		memcpy(mapped, initial_data, size);

		ID3D12Resource_Unmap(buffer, 0, NULL);
	}

	d3d12_set_debug_name((ID3D12Object *)buffer, debug_name);

	return buffer;
}

ID3D12Resource *d3d12_create_readback_buffer(ID3D12Device *device, uint32_t size, string_t debug_name)
{
	D3D12_HEAP_PROPERTIES heap_properties = {
		.Type = D3D12_HEAP_TYPE_READBACK,
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
	};

	ID3D12Resource *buffer;
	HRESULT hr = ID3D12Device_CreateCommittedResource(device,
													  &heap_properties,
													  D3D12_HEAP_FLAG_NONE,
													  &desc,
													  D3D12_RESOURCE_STATE_COMMON,
													  NULL,
													  &IID_ID3D12Resource,
													  &buffer);
	D3D12_CHECK_HR(hr, return NULL);

	d3d12_set_debug_name((ID3D12Object *)buffer, debug_name);

	return buffer;
}

void d3d12_set_debug_name(ID3D12Object *object, string_t name)
{
	if (name.count > 0) m_scoped_temp
	{
		string16_t wide = utf16_from_utf8(temp, name);
		ID3D12Object_SetName(object, wide.data);
	}
}

string_t d3d12_get_debug_name(arena_t *arena, ID3D12Object *object)
{
	UINT size = 0;
	ID3D12Object_GetPrivateData(object, &WKPDID_D3DDebugObjectNameW, &size, NULL);

	string_t result = S("<no debug name set>");

	if (size > 0)
	{
		wchar_t *data = m_alloc_nozero(arena, size, 16);
		ID3D12Object_GetPrivateData(object, &WKPDID_D3DDebugObjectNameW, &size, data);

		string16_t name_wide = {
			.data  = data,
			.count = size / sizeof(wchar_t),
		};

		result = utf8_from_utf16(arena, name_wide);
	}

	return result;
}

void d3d12_deferred_release(IUnknown *resource)
{
	d3d12_deferred_release_queue_t *queue = &g_rhi.deferred_release_queue;

	// TODO: lock-free

	mutex_lock(&queue->mutex);

	d3d12_deferred_release_t *release = &queue->queue[queue->head++ % ARRAY_COUNT(queue->queue)];
	release->resource    = resource;
	release->frame_index = g_rhi.fence_value;

	mutex_unlock(&queue->mutex);
}

void d3d12_flush_deferred_release_queue(uint32_t frame_index)
{
	d3d12_deferred_release_queue_t *queue = &g_rhi.deferred_release_queue;

	mutex_lock(&queue->mutex);

	uint32_t head = queue->head;

	mutex_unlock(&queue->mutex);

	uint32_t tail = queue->tail;

	while (tail < head)
	{
		d3d12_deferred_release_t *release = &queue->queue[tail++ % ARRAY_COUNT(queue->queue)];

		if (release->frame_index <= frame_index)
		{
			COM_SAFE_RELEASE(release->resource);
		}
		else
		{
			break;
		}
	}

	queue->tail = tail;
}

uint32_t d3d12_resource_index(rhi_resource_flags_t flags)
{
	uint32_t resource_index = (flags & RhiResourceFlag_dynamic) ? g_rhi.frame_index % g_rhi.frame_latency : 0;
	return resource_index;
}
