void d3d12_arena_init(ID3D12Device *device, d3d12_buffer_arena_t *arena, uint32_t capacity, string_t debug_name)
{
	arena->buffer   = d3d12_create_upload_buffer(device, capacity, NULL, debug_name);
	arena->gpu_base = ID3D12Resource_GetGPUVirtualAddress(arena->buffer);

	ID3D12Resource_Map(arena->buffer, 0, &(D3D12_RANGE){ 0 }, &arena->cpu_base);

	arena->at       = 0;
	arena->capacity = capacity;
}

void d3d12_arena_release(d3d12_buffer_arena_t *arena)
{
	ID3D12Resource_Unmap(arena->buffer, 0, NULL);

	COM_SAFE_RELEASE(arena->buffer);

	zero_struct(arena);
}

d3d12_buffer_allocation_t d3d12_arena_alloc(d3d12_buffer_arena_t *arena, uint32_t size, uint32_t alignment)
{
	d3d12_buffer_allocation_t result = { 0 };

	uint32_t at;
	uint32_t at_aligned;

	for (;;)
	{
		// TODO: The only reason why this is a CAS loop is because of the align - I could think of ways around that
		// I could also just have an allocator per thread, but...
		at         = atomic_load(&arena->at);
		at_aligned = align_forward(arena->at, alignment);

		if (atomic_compare_exchange_strong(&arena->at, &at, at_aligned + size))
		{
			break;
		}
	}

	if (ALWAYS(at_aligned + size <= arena->capacity))
	{
		result.offset = at_aligned;
		result.cpu    = arena->cpu_base + at_aligned;
		result.gpu    = arena->gpu_base + at_aligned;
	}

	result.buffer = arena->buffer;

	return result;
}

void d3d12_arena_reset(d3d12_buffer_arena_t *arena)
{
	arena->at = 0;
}
