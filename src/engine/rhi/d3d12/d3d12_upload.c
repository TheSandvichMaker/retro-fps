bool d3d12_upload_ring_buffer_init(ID3D12Device *device, d3d12_upload_ring_buffer_t *ring_buffer)
{
	zero_struct(ring_buffer);

	HRESULT hr;
	
	{
		D3D12_COMMAND_QUEUE_DESC desc = {
			.Type     = D3D12_COMMAND_LIST_TYPE_COPY,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		};

		hr = ID3D12Device_CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue, &ring_buffer->queue);
		D3D12_CHECK_HR(hr, return false);
	}

	for (size_t i = 0; i < RhiMaxUploadSubmissions; i++)
	{
		d3d12_upload_submission_t *submission = &ring_buffer->submissions[i];

		hr = ID3D12Device_CreateCommandAllocator(device, 
												 D3D12_COMMAND_LIST_TYPE_COPY, 
												 &IID_ID3D12CommandAllocator, 
												 &submission->allocator);
		D3D12_CHECK_HR(hr, return false);

		hr = ID3D12Device_CreateCommandList(device,
											0,
											D3D12_COMMAND_LIST_TYPE_COPY,
											submission->allocator,
											NULL,
											&IID_ID3D12GraphicsCommandList,
											&submission->command_list);
		D3D12_CHECK_HR(hr, return false);

		ID3D12GraphicsCommandList_Close(submission->command_list);
	}

	hr = ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &ring_buffer->fence);
	D3D12_CHECK_HR(hr, return false);

	ring_buffer->buffer = d3d12_create_upload_buffer(device, RhiUploadRingBufferSize, NULL, S("upload_ring_buffer"));

	void *mapped;

	hr = ID3D12Resource_Map(ring_buffer->buffer, 0, &(D3D12_RANGE){ 0 }, &mapped);
	D3D12_CHECK_HR(hr, return false);

	ring_buffer->buffer_base = mapped;

	ring_buffer->capacity = RhiUploadRingBufferSize;
	ASSERT(IS_POW2(ring_buffer->capacity));

	ring_buffer->mask = ring_buffer->capacity - 1;

	return true;
}

d3d12_upload_context_t d3d12_upload_begin(size_t size, size_t align)
{
	d3d12_upload_ring_buffer_t *ring_buffer = &g_rhi.upload_ring_buffer;

	// TODO: Handle resizing the ring buffer?
	ASSERT_MSG(size <= ring_buffer->capacity, "The upload ring buffer is too small for your upload!");

	d3d12_upload_submission_t *submission = NULL;

	uint32_t try_count = 0;

	mutex_lock(&ring_buffer->mutex);
	{
		while (!submission)
		{
			ASSERT(try_count < RhiMaxUploadSubmissions);

			size_t submissions_used      = ring_buffer->submission_head - ring_buffer->submission_tail;
			size_t submissions_available = ARRAY_COUNT(ring_buffer->submissions) - submissions_used;

			if (submissions_available > 0)
			{
				size_t aligned_head = align_forward(ring_buffer->head, align);

				size_t capacity_used      = aligned_head - ring_buffer->tail;
				size_t capacity_available = ring_buffer->capacity - capacity_used;

				if (capacity_available >= size)
				{
					size_t allocation_offset_start = (aligned_head           ) & ring_buffer->mask;
					size_t allocation_offset_end   = (aligned_head + size - 1) & ring_buffer->mask;

					if (allocation_offset_start < allocation_offset_end)
					{
						size_t submission_index = ring_buffer->submission_head % ARRAY_COUNT(ring_buffer->submissions);
						submission = &ring_buffer->submissions[submission_index];
						submission->offset = (uint32_t)allocation_offset_start;
						submission->size   = (uint32_t)size;

						ring_buffer->submission_head += 1;
					}
				}
			}

			if (!submission)
			{
				ASSERT(submissions_used > 0);

				size_t next_submission_index = ring_buffer->submission_tail % ARRAY_COUNT(ring_buffer->submissions);
				d3d12_upload_submission_t *next_submission = &ring_buffer->submissions[next_submission_index];

				uint64_t fence_value = ID3D12Fence_GetCompletedValue(ring_buffer->fence);

				if (fence_value < next_submission->fence_value)
				{
					ID3D12Fence_SetEventOnCompletion(ring_buffer->fence, next_submission->fence_value, NULL);
				}

				ring_buffer->submission_tail += 1;
				ring_buffer->tail            += submission->size;
			}

			try_count += 1;
		}
	}
	mutex_unlock(&ring_buffer->mutex);

	ASSERT(submission);

	ID3D12CommandAllocator    *allocator    = submission->allocator;
	ID3D12GraphicsCommandList *command_list = submission->command_list;

	ID3D12CommandAllocator_Reset   (allocator);
	ID3D12GraphicsCommandList_Reset(command_list, allocator, NULL);

	d3d12_upload_context_t ctx = {
		.buffer       = ring_buffer->buffer,
		.command_list = command_list,
		.offset       = submission->offset,
		.pointer      = ring_buffer->buffer_base + submission->offset,
		.submission   = submission,
	};

	return ctx;
}

void d3d12_upload_end(const d3d12_upload_context_t *ctx)
{
	ASSERT(ctx);

	d3d12_upload_ring_buffer_t *ring_buffer = &g_rhi.upload_ring_buffer;

	mutex_lock(&ring_buffer->mutex);
	{
		d3d12_upload_submission_t *submission = ctx->submission;
		submission->fence_value = ++ring_buffer->fence_value;

		ID3D12GraphicsCommandList *command_list = submission->command_list;
		ID3D12GraphicsCommandList_Close(command_list);

		ID3D12CommandList *lists[] = { (ID3D12CommandList *)command_list };
		ID3D12CommandQueue_ExecuteCommandLists(ring_buffer->queue, 1, lists);

		HRESULT hr = ID3D12CommandQueue_Signal(ring_buffer->queue, ring_buffer->fence, submission->fence_value);
		D3D12_CHECK_HR(hr, (void)0);
	}
	mutex_unlock(&ring_buffer->mutex);
}
