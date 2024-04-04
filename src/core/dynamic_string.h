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

DREAM_INLINE bool dyn_string_insertc(dynamic_string_t *d, size_t index, char c)
{
	bool result = false;

	if (d->count < d->capacity && index < d->capacity)
	{
		for (int64_t i = d->count; i > (int64_t)index; i--)
		{
			d->data[i] = d->data[i - 1];
		}
		d->data[index] = c;
		d->count = MAX(d->count + 1, index + 1);
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

DREAM_INLINE void dyn_string_remove_range(dynamic_string_t *d, size_t start, size_t count)
{
	if (d->count == 0)
	{
		return;
	}

	start = MIN(start, d->count);

	size_t remove_count = MIN(count, d->count - start);

	for (size_t i = start; i < d->count - remove_count; i++)
	{
		d->data[i] = d->data[i + count];
	}

	d->count -= remove_count;
}

DREAM_INLINE void dyn_string_remove_at(dynamic_string_t *d, size_t index)
{
	if (index >= d->count)
	{
		return;
	}

	dyn_string_remove_range(d, index, 1);
}

#endif
