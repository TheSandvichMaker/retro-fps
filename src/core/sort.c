// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

void radix_sort_u32(uint32_t *array, size_t count)
{
    m_scoped_temp
    {
        uint32_t *src = array;
        uint32_t *dst = m_alloc_array_nozero(temp, count, uint32_t);

        for (int byte_index = 0; byte_index < 4; byte_index++)
        {
            uint32_t offsets[256] = { 0 };

            for (size_t i = 0; i < count; i++)
            {
                unsigned char radix_piece = (unsigned char)(src[i] >> 8*byte_index);
                offsets[radix_piece]++;
            }

            uint32_t total = 0;
            for (size_t i = 0; i < ARRAY_COUNT(offsets); i++)
            {
                uint32_t piece_count = offsets[i];
                offsets[i] = total;
                total += piece_count;
            }

            for (size_t i = 0; i < count; i++)
            {
                unsigned char radix_piece = (unsigned char)(src[i] >> 8*byte_index);
                dst[offsets[radix_piece]++] = src[i];
            }

            SWAP(uint32_t *, dst, src);
        }
    }
}

void radix_sort_u64(uint64_t *array, size_t count)
{
    m_scoped_temp
    {
        uint64_t *src = array;
        uint64_t *dst = m_alloc_array_nozero(temp, count, uint64_t);

        for (int byte_index = 0; byte_index < sizeof(*array); byte_index++)
        {
            size_t offsets[256] = { 0 };

            for (size_t i = 0; i < count; i++)
            {
                unsigned char radix_piece = (unsigned char)(src[i] >> 8*byte_index);
                offsets[radix_piece]++;
            }

            uint64_t total = 0;
            for (size_t i = 0; i < ARRAY_COUNT(offsets); i++)
            {
                uint64_t piece_count = offsets[i];
                offsets[i] = total;
                total += piece_count;
            }

            for (size_t i = 0; i < count; i++)
            {
                unsigned char radix_piece = (unsigned char)(src[i] >> 8*byte_index);
                dst[offsets[radix_piece]++] = src[i];
            }

            SWAP(uint64_t *, dst, src);
        }
    }
}

void radix_sort_keys(sort_key_t *array, size_t count)
{
	radix_sort_u64((uint64_t *)array, count);
}

fn_local void merge_sort_internal(char *a, char *b, size_t count, size_t element_size, comparison_function_t func, void *user_data)
{
	if (count <= 1)
	{
		return;
	}

	size_t count_l = count / 2;
	size_t count_r = count - count_l;

	size_t size   = count  *element_size;
	size_t size_l = count_l*element_size;

	merge_sort_internal(b         , a         , count_l, element_size, func, user_data);
	merge_sort_internal(b + size_l, a + size_l, count_r, element_size, func, user_data);

	char *middle = b + size_l;
	char *end    = b + size;

	char *l = b;
	char *r = b + size_l;

	char *out = a;
	for (size_t i = 0; i < count; i++)
	{
		if ((l <  middle) && 
			(r >= end || func(l, r, user_data) <= 0))
		{
			memcpy(out, l, element_size);
			out += element_size;
			l   += element_size;
		}
		else
		{
			memcpy(out, r, element_size);
			out += element_size;
			r   += element_size;
		}
	}
}

void merge_sort(void *a, size_t count, size_t element_size, comparison_function_t func, void *user_data)
{
	m_scoped_temp
	{
		void *b = m_copy(temp, a, count*element_size);
		merge_sort_internal(a, b, count, element_size, func, user_data);
	}
}