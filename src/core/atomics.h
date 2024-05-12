// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

// ------------------------------------------------------------------
// Manual declaration of MSVC intrinsics to avoid including gunky 
// headers

int64_t _InterlockedExchangeAdd64(int64_t volatile *addend, int64_t value);
#pragma intrinsic(_InterlockedExchangeAdd64)

long _InterlockedExchangeAdd(long volatile *addend, long value);
#pragma intrinsic(_InterlockedExchangeAdd)

int64_t _InterlockedCompareExchange64(int64_t volatile *destination, int64_t exchange, int64_t comparand);
#pragma intrinsic(_InterlockedCompareExchange64)

int64_t _InterlockedExchange64(int64_t volatile *destination, int64_t exchange);
#pragma intrinsic(_InterlockedExchange64)

long _InterlockedIncrement(long volatile *destination);
#pragma intrinsic(_InterlockedIncrement)

long _InterlockedDecrement(long volatile *destination);
#pragma intrinsic(_InterlockedDecrement)

long _InterlockedExchange(long volatile *destination, long exchange);
#pragma intrinsic(_InterlockedCompareExchange)

long _InterlockedOr(long volatile *destination, long value);
#pragma intrinsic(_InterlockedOr)


// ------------------------------------------------------------------

fn_local int64_t atomic_add_i64(int64_t volatile *addend, int64_t value)
{
    return _InterlockedExchangeAdd64(addend, value);
}

fn_local int32_t atomic_add_i32(int32_t volatile *addend, int32_t value)
{
    return _InterlockedExchangeAdd((long volatile *)addend, value);
}

fn_local uint64_t atomic_add_u64(uint64_t volatile *addend, int64_t value)
{
    return _InterlockedExchangeAdd64((int64_t volatile *)addend, value);
}

fn_local uint32_t atomic_add_u32(uint32_t volatile *addend, int32_t value)
{
    return _InterlockedExchangeAdd((long volatile *)addend, value);
}

fn_local int64_t atomic_cas_i64(int64_t volatile *destination, int64_t exchange, int64_t comparand)
{
    return _InterlockedCompareExchange64(destination, exchange, comparand);
}

typedef void *void_t;

fn_local void *atomic_cas_ptr(void_t volatile *destination, void *exchange, void *comparand)
{
    return (void *)_InterlockedCompareExchange64((int64_t volatile *)destination, (intptr_t)exchange, (intptr_t)comparand);
}

fn_local uint64_t atomic_exchange_u64(uint64_t volatile *target, uint64_t value)
{
    return (uint64_t)_InterlockedExchange64((int64_t volatile *)target, (int64_t)value);
}

fn_local int64_t atomic_exchange_i64(int64_t volatile *target, int64_t value)
{
    return _InterlockedExchange64(target, value);
}

fn_local long atomic_increment(long volatile *target)
{
    return _InterlockedIncrement(target);
}

fn_local uint32_t atomic_decrement_u32(uint32_t volatile *target)
{
    return _InterlockedDecrement((long volatile *)target);
}

fn_local uint32_t atomic_cas_u32(uint32_t volatile *destination, uint32_t exchange, uint32_t comparand)
{
    return _InterlockedCompareExchange((long volatile *)destination, exchange, comparand);
}

fn_local uint32_t atomic_increment_u32(uint32_t volatile *target)
{
    return (uint32_t)_InterlockedIncrement((long volatile *)target);
}

fn_local uint32_t atomic_or_u32(uint32_t volatile *target, uint32_t value)
{
	uint32_t result = _InterlockedOr((long volatile *)target, (long)value);
	return result;
}

fn_local uint32_t atomic_load_u32(uint32_t volatile *target)
{
	return *target;
}

fn_local void atomic_store_u32(uint32_t volatile *target, uint32_t value)
{
	*target = value;
}

fn_local uint64_t atomic_load_u64(uint64_t volatile *target)
{
	return *target;
}

fn_local void atomic_store_u64(uint64_t volatile *target, uint64_t value)
{
	*target = value;
}

#define COMPILER_BARRIER _ReadWriteBarrier()
