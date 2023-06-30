#ifndef ATOMICS_H
#define ATOMICS_H

#include <stdint.h>

// ------------------------------------------------------------------
// Manual declaration of MSVC intrinsics to avoid including gunky 
// headers

int64_t _InterlockedExchangeAdd64(int64_t volatile *addend, int64_t value);
#pragma intrinsic(_InterlockedExchangeAdd64)

int64_t _InterlockedCompareExchange64(int64_t volatile *destination, int64_t exchange, int64_t comparand);
#pragma intrinsic(_InterlockedCompareExchange64)

int64_t _InterlockedExchange64(int64_t volatile *destination, int64_t exchange);
#pragma intrinsic(_InterlockedExchange64)

long _InterlockedIncrement(long volatile *destination);
#pragma intrinsic(_InterlockedIncrement)

long _InterlockedExchange(long volatile *destination, long exchange);
#pragma intrinsic(_InterlockedCompareExchange)


// ------------------------------------------------------------------

static inline int64_t atomic_add64(int64_t volatile *addend, int64_t value)
{
    return _InterlockedExchangeAdd64((long long volatile *)addend, value) - value;
}

static inline int64_t atomic_cas64(int64_t volatile *destination, int64_t exchange, int64_t comparand)
{
    return _InterlockedCompareExchange64(destination, exchange, comparand);
}

static inline int64_t atomic_exchange64(int64_t volatile *target, int64_t value)
{
    return _InterlockedExchange64(target, value);
}

static inline long atomic_increment(long volatile *target)
{
    return _InterlockedIncrement(target);
}

static inline uint32_t atomic_cas_u32(uint32_t volatile *destination, uint32_t exchange, uint32_t comparand)
{
    return _InterlockedCompareExchange((long volatile *)destination, exchange, comparand);
}

static inline uint32_t atomic_increment_u32(uint32_t volatile *target)
{
    return (uint32_t)_InterlockedIncrement((long volatile *)target);
}

#define COMPILER_BARRIER _ReadWriteBarrier()

#endif /* ATOMICS_H */
