#ifndef DREAM_ARRAY_H
#define DREAM_ARRAY_H

#include "core/api_types.h"

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

#endif
