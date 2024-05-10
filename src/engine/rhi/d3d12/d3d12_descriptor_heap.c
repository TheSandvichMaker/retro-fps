void d3d12_descriptor_arena_init(ID3D12Device *device,
								 d3d12_descriptor_arena_t *arena, 
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

	hr = ID3D12Device_CreateDescriptorHeap(device, &desc, &IID_ID3D12DescriptorHeap, &arena->heap);
	D3D12_CHECK_HR(hr, return); // TODO: think about proper error (non-)recovery

	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(arena->heap, &arena->cpu_base);

	if (shader_visible)
	{
		ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(arena->heap, &arena->gpu_base);
	}

	arena->at       = 1; // 0 is reserved to indicate a null descriptor
	arena->capacity = capacity;
	arena->stride   = ID3D12Device_GetDescriptorHandleIncrementSize(device, type);
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

//
// Descriptor Heap 
//

void d3d12_descriptor_heap_init(ID3D12Device *device,
								arena_t *rhi_arena,
								d3d12_descriptor_heap_t *heap, 
								D3D12_DESCRIPTOR_HEAP_TYPE type, 
								UINT persistent_capacity, 
								UINT transient_capacity, 
								bool shader_visible,
								string_t debug_name)
{
	(void)transient_capacity;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {
		.Type           = type,
		.NumDescriptors = persistent_capacity + 1,
		.Flags          = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : 0,
	};

	HRESULT hr;

	hr = ID3D12Device_CreateDescriptorHeap(device, &desc, &IID_ID3D12DescriptorHeap, &heap->heap);
	D3D12_CHECK_HR(hr, return); // TODO: think about proper error (non-)recovery

	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(heap->heap, &heap->cpu_base);

	if (shader_visible)
	{
		ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(heap->heap, &heap->gpu_base);
	}

	heap->debug_name = string_copy(rhi_arena, debug_name);

	heap->frame_index = 0;

	heap->capacity = persistent_capacity;
	heap->stride   = ID3D12Device_GetDescriptorHandleIncrementSize(device, type);

	heap->free_count   = persistent_capacity;
	heap->free_indices = m_alloc_array_nozero(rhi_arena, heap->free_count, uint32_t);

	for (size_t i = 0; i < heap->free_count; i++)
	{
		heap->free_indices[i] = (uint32_t)(i + 1);
	}

	heap->pending_free_count   = 0;
	heap->pending_free_indices = m_alloc_array_nozero(rhi_arena, heap->capacity, d3d12_pending_free_t);
}

void d3d12_descriptor_heap_release(d3d12_descriptor_heap_t *heap)
{
	COM_SAFE_RELEASE(heap->heap);
}

void d3d12_descriptor_heap_flush_pending_frees(d3d12_descriptor_heap_t *heap, uint32_t frame_index)
{
	mutex_lock(&heap->mutex);

	uint32_t pending_free_count = heap->pending_free_count;
	uint32_t new_free_count     = heap->free_count;

	for (size_t i = 0; i < pending_free_count; i++)
	{
		d3d12_pending_free_t *pending = &heap->pending_free_indices[i];

		if (pending->frame_index <= frame_index)
		{
			heap->free_indices[new_free_count++] = pending->index;
		}
		else
		{
			break;
		}
	}

	heap->free_count  = new_free_count;
	heap->frame_index = frame_index;

	mutex_unlock(&heap->mutex);
}

d3d12_descriptor_t d3d12_allocate_descriptor_persistent(d3d12_descriptor_heap_t *heap)
{
	mutex_lock(&heap->mutex);

	uint32_t index = --heap->free_count;

	mutex_unlock(&heap->mutex);

	if (index >= heap->capacity)
	{
		FATAL_ERROR("Ran out of persistent descriptors in descriptor heap '%.*s'!", Sx(heap->debug_name));
	}

	d3d12_descriptor_t result = {
		.cpu   = { heap->cpu_base.ptr + heap->stride * index },
		.gpu   = { heap->gpu_base.ptr + heap->stride * index },
		.index = index,
	};

	return result;
}

void d3d12_free_descriptor_persistent(d3d12_descriptor_heap_t *heap, uint32_t index)
{
	if (ALWAYS(index < heap->capacity))
	{
		mutex_lock(&heap->mutex);

		uint32_t pending_index = heap->pending_free_count++;

		heap->pending_free_indices[pending_index] = (d3d12_pending_free_t){
			.index       = index,
			.frame_index = heap->frame_index,
		};

		mutex_unlock(&heap->mutex);
	}
}
