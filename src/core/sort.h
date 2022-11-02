#ifndef SORT_H
#define SORT_H

#include "api_types.h"

void radix_sort_u32(uint32_t *array, size_t size);
void radix_sort_u64(uint64_t *array, size_t size);

#endif /* SORT_H */
