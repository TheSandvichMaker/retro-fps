// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

void radix_sort_u32(uint32_t *array, size_t count)
{

    m_scoped_temp
    {
        uint32_t *src = array;
        uint32_t *dst = m_alloc_array(temp, count, uint32_t);

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
        uint64_t *dst = m_alloc_array(temp, count, uint64_t);

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
