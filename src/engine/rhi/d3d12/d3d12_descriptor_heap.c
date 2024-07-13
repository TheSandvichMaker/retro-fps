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

	heap->debug_name = m_copy_string(rhi_arena, debug_name);

	heap->capacity = persistent_capacity;
	heap->stride   = ID3D12Device_GetDescriptorHandleIncrementSize(device, type);

	heap->free_count   = persistent_capacity;
	heap->free_indices = m_alloc_array_nozero(rhi_arena, heap->free_count, uint32_t);

	for (size_t i = 0; i < heap->free_count; i++)
	{
		heap->free_indices[i] = (uint32_t)(heap->capacity - i);
	}

	heap->pending_free_tail    = 0;
	heap->pending_free_head    = 0;
	heap->pending_free_indices = m_alloc_array_nozero(rhi_arena, heap->capacity, d3d12_pending_free_t);

#if DREAM_SLOW
	heap->debug_buffer_map  = m_alloc_array(rhi_arena, heap->capacity, rhi_buffer_t);
	heap->debug_texture_map = m_alloc_array(rhi_arena, heap->capacity, rhi_texture_t);
#endif
}

void d3d12_descriptor_heap_release(d3d12_descriptor_heap_t *heap)
{
	COM_SAFE_RELEASE(heap->heap);
}

void d3d12_descriptor_heap_flush_pending_frees(d3d12_descriptor_heap_t *heap, uint32_t frame_index)
{
	mutex_lock(&heap->mutex);

	uint32_t new_free_count = heap->free_count;

	for (size_t i = heap->pending_free_tail; i < heap->pending_free_head; i++)
	{
		d3d12_pending_free_t *pending = &heap->pending_free_indices[i % heap->capacity];

		if (pending->frame_index <= frame_index)
		{
			log(RHI_D3D12, SuperSpam, 
				"Freeing descriptor %u which was set to be freed at frame index %llu, current frame index: %llu", 
				pending->index, pending->frame_index, frame_index);

			heap->free_indices[new_free_count++] = pending->index;
			heap->pending_free_tail += 1;
		}
		else
		{
			break;
		}
	}

	heap->free_count  = new_free_count;

	mutex_unlock(&heap->mutex);
}

d3d12_descriptor_t d3d12_allocate_descriptor_persistent(d3d12_descriptor_heap_t *heap)
{
	mutex_lock(&heap->mutex);

	uint32_t index = heap->free_indices[--heap->free_count];
	heap->used += 1;

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

	log(RHI_D3D12, SuperSpam, "Allocated persistent descriptor %u from heap: %cs", result.index, heap->debug_name);

	return result;
}

d3d12_descriptor_t d3d12_allocate_descriptor_persistent_for_buffer(d3d12_descriptor_heap_t *heap, rhi_buffer_t buffer)
{
	(void)buffer;

	d3d12_descriptor_t result = d3d12_allocate_descriptor_persistent(heap);
#if DREAM_SLOW
	mutex_scoped_lock(&heap->mutex)
	{
		// Nobody should be using this!!!!
		ASSERT(!RESOURCE_HANDLE_VALID(heap->debug_buffer_map [result.index]));
		ASSERT(!RESOURCE_HANDLE_VALID(heap->debug_texture_map[result.index]));

		heap->debug_buffer_map[result.index] = buffer;
	}
#endif
	return result;
}

d3d12_descriptor_t d3d12_allocate_descriptor_persistent_for_texture(d3d12_descriptor_heap_t *heap, rhi_texture_t texture)
{
	(void)texture;

	d3d12_descriptor_t result = d3d12_allocate_descriptor_persistent(heap);
#if DREAM_SLOW
	mutex_scoped_lock(&heap->mutex)
	{
		// Nobody should be using this!!!!
		ASSERT(!RESOURCE_HANDLE_VALID(heap->debug_buffer_map [result.index]));
		ASSERT(!RESOURCE_HANDLE_VALID(heap->debug_texture_map[result.index]));

		heap->debug_texture_map[result.index] = texture;
	}
#endif
	return result;
}

void d3d12_free_descriptor_persistent(d3d12_descriptor_heap_t *heap, uint32_t index)
{
	if (index > 0 && ALWAYS(index < heap->capacity))
	{
		mutex_lock(&heap->mutex);

		uint32_t pending_index = heap->pending_free_head++ % heap->capacity; // TODO: mask pow2

		heap->pending_free_indices[pending_index] = (d3d12_pending_free_t){
			.index       = index,
			.frame_index = g_rhi.fence_value + g_rhi.frame_latency,
		};

		log(RHI_D3D12, SuperSpam, "Deferred descriptor %u to be freed at frame index %llu, current frame index: %llu", index, g_rhi.fence_value + g_rhi.frame_latency, g_rhi.frame_index);

#if DREAM_SLOW
		NULLIFY_HANDLE(&heap->debug_buffer_map [index]);
		NULLIFY_HANDLE(&heap->debug_texture_map[index]);
#endif

		heap->used -= 1;

		mutex_unlock(&heap->mutex);
	}
}
