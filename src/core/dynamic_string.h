#ifndef DREAM_CORE_DYNAMIC_STRING_H
#define DREAM_CORE_DYNAMIC_STRING_H

#include "core/api_types.h"

DREAM_INLINE size_t dyn_string_space_remaining(dynamic_string_t *d)
{
	size_t result = 0;

	if (ALWAYS(d->count <= d->capacity))
	{
		result = (d->capacity - d->count);
	}

	return result;
}

DREAM_INLINE bool dyn_string_appendc(dynamic_string_t *d, char c)
{
	bool result = false;

	if (d->count < d->capacity)
	{
		d->data[d->count++] = c;
		result = true;
	}

	return result;
}

DREAM_INLINE bool dyn_string_appends(dynamic_string_t *d, string_t s)
{
	size_t space = dyn_string_space_remaining(d);
	size_t append_count = MIN(s.count, space);

	bool result = (append_count == s.count);

	for (size_t i = 0; i < append_count; i++)
	{
		d->data[d->count++] = s.data[i];
	}

	return result;
}

DREAM_INLINE void dyn_string_clear(dynamic_string_t *d)
{
	d->count = 0;
}

#endif
