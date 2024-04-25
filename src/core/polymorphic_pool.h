#pragma once

#ifndef POOL_FREE_BIT
#define POOL_FREE_BIT (1 << 31)
#endif

#define pool_index_valid(pool, index) (!((pool)->generations[index] & POOL_FREE_BIT))
#define for_pool(it, pool) \
	for (size_t it = 1; it < (pool)->watermark; it++) \
		if (pool_index_valid(pool, it))

#define DEFINE_POOL(type_t, name, capacity)                                           \
	static_assert(sizeof(type_t) >= sizeof(uint32_t),                                 \
				  "Pooled types need to be at least 4 bytes!");                       \
	typedef struct name##_pool_slot_t                                                 \
	{                                                                                 \
		union                                                                         \
		{                                                                             \
			uint32_t next_free;                                                       \
			type_t item;                                                              \
		};                                                                            \
	} name##_pool_slot_t;                                                             \
	                                                                                  \
	typedef struct name##_pool_t                                                      \
	{                                                                                 \
		uint32_t watermark;                                                           \
		uint32_t count;                                                               \
		uint32_t first_free;                                                          \
		uint32_t generations[capacity];                                               \
		union                                                                         \
		{                                                                             \
			name##_pool_slot_t slots[capacity];                                       \
			type_t items[capacity];                                                   \
		};                                                                            \
	} name##_pool_t;					                                              \
                                                                                      \
	DEFINE_HANDLE_TYPE(name##_handle_t)                                               \
	                                                                                  \
fn_local name##_handle_t name##_handle_from_index(name##_pool_t *pool, size_t index)  \
{                                                                                     \
	name##_handle_t result = {0};                                                     \
	if (ALWAYS(index <= pool->watermark))                                             \
	{                                                                                 \
		result.index = (uint32_t)index;                                               \
		result.generation = pool->generations[index];                                 \
	}                                                                                 \
	return result;                                                                    \
}                                                                                     \
                                                                                      \
fn_local name##_handle_t name##_pool_allocate(name##_pool_t *pool, type_t **out_item) \
{                                                                                     \
	name##_handle_t result = {0};                                                     \
                                                                                      \
	uint32_t index = pool->first_free;                                                \
                                                                                      \
	if (!index && pool->watermark + 1 < ARRAY_COUNT(pool->slots))                     \
	{                                                                                 \
		index = ++pool->watermark;                                                    \
	}                                                                                 \
                                                                                      \
	if (index != 0)                                                                   \
	{                                                                                 \
		name##_pool_slot_t *slot = &pool->slots[index];                               \
		pool->first_free = slot->next_free;                                           \
                                                                                      \
		pool->generations[index] &= ~POOL_FREE_BIT;                                   \
                                                                                      \
		result = (name##_handle_t){                                                   \
			.index      = index,                                                      \
			.generation = pool->generations[index],                                   \
		};                                                                            \
                                                                                      \
		if (out_item)                                                                 \
		{                                                                             \
			*out_item = &slot->item;                                                  \
		}                                                                             \
                                                                                      \
		pool->count++;                                                                \
	}                                                                                 \
                                                                                      \
	return result;                                                                    \
}                                                                                     \
                                                                                      \
fn_local void name##_pool_free(name##_pool_t *pool, name##_handle_t handle)           \
{                                                                                     \
	size_t index = handle.index;                                                      \
                                                                                      \
	if (ALWAYS(index <= pool->watermark))                                             \
	{                                                                                 \
		name##_pool_slot_t *slot = &pool->slots[handle.index];                        \
		if (ALWAYS(pool->generations[index] == handle.generation))                    \
		{                                                                             \
			slot->next_free = pool->first_free;                                       \
			pool->first_free = (uint32_t)index;                                       \
                                                                                      \
			pool->generations[index] = (handle.generation + 1) | POOL_FREE_BIT;       \
			zero_struct(&slot->item);                                                 \
                                                                                      \
			pool->count--;                                                            \
		}                                                                             \
	}                                                                                 \
}
