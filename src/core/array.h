// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#define ARRAY_RESERVE_SIZE      GB(8)
#define ARRAY_CHUNK_COMMIT_SIZE MB(1)

#define array_t(Type)    \
	struct               \
	{                    \
		size_t comitted; \
		size_t count;    \
		Type  *items;    \
	}

#define array_add(Array, Item) (array_ensure_size(Array, 1), (Array).items[(Array).count++] = Item)
