void simple_heap_init(simple_heap_t *heap, arena_t *arena)
{
	heap->arena = arena;

	for (size_t i = 0; i < ARRAY_COUNT(heap->free_block_lists); i++)
	{
		simple_heap_block_header_t *list = &heap->free_block_lists[i];
		list->next = list;
		list->prev = list;
	}
}

void *simple_heap_alloc_nozero(simple_heap_t *heap, uint16_t size)
{
	ASSERT_MSG(heap->arena, "Please initialize the heap first");

	void *result = NULL;

	if (size == 0)
	{
		return NULL;
	}

	// start at 16 bytes, otherwise we couldn't fit the freelist pointers
	uint64_t block_size       = next_pow2(MAX(16, size));
	uint64_t block_list_index = count_trailing_zeros_u64(block_size) - 5;

	if (ALWAYS(block_list_index < ARRAY_COUNT(heap->free_block_lists)))
	{
		simple_heap_block_header_t *list = &heap->free_block_lists[block_list_index];

		if (list->next != list)
		{
			simple_heap_block_header_t *block = list->next;
			block->next->prev = block->prev;
			block->prev->next = block->next;

			result = block;
		}
		else
		{
			result = m_alloc_nozero(heap->arena, block_size, 16);
		}
	}

	return result;
}

void *simple_heap_alloc(simple_heap_t *heap, uint16_t size)
{
	ASSERT_MSG(heap->arena, "Please initialize the heap first");

	void *result = simple_heap_alloc_nozero(heap, size);
	zero_memory(result, size);

	return result;
}

void simple_heap_free(simple_heap_t *heap, void *pointer, uint16_t size)
{
	ASSERT_MSG(heap->arena, "Please initialize the heap first");

	if (pointer == NULL || size == 0)
	{
		return;
	}

	// start at 16 bytes, otherwise we couldn't fit the freelist pointers
	uint64_t block_size       = next_pow2(MAX(16, size));
	uint64_t block_list_index = count_trailing_zeros_u64(block_size) - 5;

	if (ALWAYS(block_list_index < ARRAY_COUNT(heap->free_block_lists)))
	{
		simple_heap_block_header_t *list  = &heap->free_block_lists[block_list_index];
		simple_heap_block_header_t *block = pointer;

		block->next = list->next;
		block->prev = list;
		block->next->prev = block;
		block->prev->next = block;
	}
}
