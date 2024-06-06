// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#ifndef INTRIN_H
#define INTRIN_H

#include <intrin.h>

static inline uint64_t count_set_bits64(uint64_t x)
{
#if _MSC_VER
    return __popcnt64(x);
#else
#error TODO: Implement
#endif
}

static inline void bit_scan_forward64(unsigned long *index, uint64_t mask)
{
#if _MSC_VER
    _BitScanForward64(index, mask);
#else
#error TODO: Implement
#endif
}

fn_local unsigned long bit_scan_reverse_u64(uint64_t x)
{
#if _MSC_VER
	unsigned long index;
    _BitScanReverse64(&index, x);
#else
#error TODO: Implement
#endif
	return index;
}

fn_local uint64_t count_leading_zeros_u64(uint64_t x)
{
#if _MSC_VER
	return __lzcnt64(x);
#else
#error TODO: Implement
#endif
}

fn_local uint64_t count_trailing_zeros_u64(uint64_t x)
{
	return 64 - count_leading_zeros_u64(x);
}

#define read_cpu_timer __rdtsc

#endif /* INTRIN_H */
