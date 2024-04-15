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

static inline void bit_scan_reverse64(unsigned long *index, uint64_t mask)
{
#if _MSC_VER
    _BitScanReverse64(index, mask);
#else
#error TODO: Implement
#endif
}

#define read_cpu_timer __rdtsc

#endif /* INTRIN_H */
