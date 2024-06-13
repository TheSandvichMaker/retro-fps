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

		d3d12_set_debug_name((ID3D12Object *)ring_buffer->queue, S("upload_ring_buffer_copy_command_queue"));
	}

	for (size_t i = 0; i < RhiMaxUploadSubmissions; i++)
	{
		d3d12_upload_submission_t *submission = &ring_buffer->submissions[i];

		hr = ID3D12Device_CreateCommandAllocator(device, 
												 D3D12_COMMAND_LIST_TYPE_COPY, 
												 &IID_ID3D12CommandAllocator, 
												 &submission->allocator);
		D3D12_CHECK_HR(hr, return false);

		d3d12_set_debug_name((ID3D12Object *)submission->allocator, Sf("upload_ring_buffer_submission_%zu_allocator", i));

		hr = ID3D12Device_CreateCommandList(device,
											0,
											D3D12_COMMAND_LIST_TYPE_COPY,
											submission->allocator,
											NULL,
											&IID_ID3D12GraphicsCommandList,
											&submission->command_list);
		D3D12_CHECK_HR(hr, return false);

		d3d12_set_debug_name((ID3D12Object *)submission->command_list, Sf("upload_ring_buffer_submission_%zu_command_list", i));

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

	log(RHI_D3D12, SuperSpam, "Starting ring buffer upload (size: %zu, align: %zu)", size, align);

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
						ring_buffer->head             = submission->offset + submission->size;

						log(RHI_D3D12, SuperSpam, "Acquired ring buffer upload submission %zu (offset: %zu, size: %zu)", 
							submission_index, 
							allocation_offset_start,
							size);
					}
				}
			}

			if (!submission)
			{
				ASSERT(submissions_used > 0);

				// TODO: Don't necessarily need to retire ring buffer submissions in order... But it's simpler that way.

				log(RHI_D3D12, SuperSpam, "Could not get a free ring buffer submission, attempting to retire");

				size_t retired_submission_index = ring_buffer->submission_tail % ARRAY_COUNT(ring_buffer->submissions);
				d3d12_upload_submission_t *retired_submission = &ring_buffer->submissions[retired_submission_index];

				uint64_t fence_value = ID3D12Fence_GetCompletedValue(ring_buffer->fence);

				if (fence_value < retired_submission->fence_value)
				{
					log(RHI_D3D12, SuperSpam, "Waiting for ring buffer submission %zu to finish", retired_submission_index);
					ID3D12Fence_SetEventOnCompletion(ring_buffer->fence, retired_submission->fence_value, NULL);
				}

				log(RHI_D3D12, SuperSpam, "Retired ring buffer submission %zu", retired_submission_index);

				ring_buffer->submission_tail += 1;
				ring_buffer->tail             = retired_submission->offset + retired_submission->size;
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

uint64_t d3d12_upload_end(const d3d12_upload_context_t *ctx)
{
	ASSERT(ctx);

	d3d12_upload_ring_buffer_t *ring_buffer = &g_rhi.upload_ring_buffer;

	d3d12_upload_submission_t *submission = ctx->submission;

	uint64_t fence_value = 0;

	mutex_lock(&ring_buffer->mutex);
	{
		fence_value = ++ring_buffer->fence_value;

		submission->fence_value = fence_value;

		log(RHI_D3D12, SuperSpam, "Finished recording ring buffer upload, submitting with fence value %u", fence_value);

		ID3D12GraphicsCommandList *command_list = submission->command_list;
		ID3D12GraphicsCommandList_Close(command_list);

		ID3D12CommandList *lists[] = { (ID3D12CommandList *)command_list };
		ID3D12CommandQueue_ExecuteCommandLists(ring_buffer->queue, 1, lists);

		ID3D12CommandQueue_Signal(ring_buffer->queue, ring_buffer->fence, fence_value);
	}
	mutex_unlock(&ring_buffer->mutex);

	return fence_value;
}

void d3d12_flush_ring_buffer_uploads(void)
{
	d3d12_upload_ring_buffer_t *ring_buffer = &g_rhi.upload_ring_buffer;

	uint64_t fence_value = ID3D12Fence_GetCompletedValue(ring_buffer->fence);

	if (fence_value < ring_buffer->fence_value)
	{
		ID3D12Fence_SetEventOnCompletion(ring_buffer->fence, ring_buffer->fence_value, NULL);
	}
}
