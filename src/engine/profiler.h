// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#ifndef DREAM_PROFILER
#define DREAM_PROFILER 1
#endif

typedef struct profiler_state_t
{
	uint64_t start_tsc;
	uint64_t end_tsc;
} profiler_state_t;

global profiler_state_t profiler;

#if DREAM_PROFILER

typedef struct profiler_slot_t
{
	const char *tag;               // 8
	uint64_t hit_count;            // 16
	uint64_t exclusive_tsc;        // 24
	uint64_t inclusive_tsc;        // 32
	uint64_t bytes_processed;      // 40
	double   display_exclusive_ms; // 48
	double   display_inclusive_ms; // 56
} profiler_slot_t;

typedef struct profiler_block_t
{
	const char *tag;
	uint32_t id;
	uint32_t parent_id;
	uint64_t start_tsc;
	uint64_t start_inclusive_tsc;
	uint64_t bytes_processed;
} profiler_block_t;

global uint32_t         profiler_parent;
extern uint64_t         profiler_slots_count;
extern profiler_slot_t  profiler_slots[];
extern profiler_slot_t  profiler_slots_read[];

#define CREATE_PROFILER_TABLE                               \
	enum { ProfilerSlotCount = __COUNTER__ + 1 };           \
    profiler_slot_t profiler_slots     [ProfilerSlotCount]; \
    profiler_slot_t profiler_slots_read[ProfilerSlotCount]; \
    uint64_t profiler_slots_count = ProfilerSlotCount;

#define PROF__VAR(var) PASTE(Prof__, var)
#define PROF__ID (__COUNTER__ + 1)

#define PROFILE_BEGIN_INTERNAL_(in_variable, in_tag, in_id, in_bytes)     \
	profiler_block_t in_variable = {                                      \
		.tag                 = in_tag,                                    \
		.id                  = in_id,                                     \
		.parent_id           = profiler_parent,                           \
		.start_tsc           = read_cpu_timer(),                          \
		.start_inclusive_tsc = profiler_slots[in_id].inclusive_tsc,       \
		.bytes_processed     = in_bytes,                                  \
	};                                                                    \
	profiler_parent = in_variable.id;

#define PROFILE_END_(variable)                                            \
	{                                                                     \
		uint64_t now     = read_cpu_timer();                              \
		uint64_t elapsed = now - variable.start_tsc;                      \
                                                                          \
		profiler_slot_t *slot = &profiler_slots[variable.id];             \
		slot->tag              = variable.tag;                            \
		slot->exclusive_tsc   += elapsed;                                 \
		slot->inclusive_tsc    = variable.start_inclusive_tsc + elapsed;  \
		slot->hit_count       += 1;                                       \
		slot->bytes_processed += variable.bytes_processed;                \
                                                                          \
		if (variable.parent_id)                                           \
		{                                                                 \
			profiler_slots[variable.parent_id].exclusive_tsc -= elapsed;  \
		}                                                                 \
		                                                                  \
		profiler_parent = variable.parent_id;                             \
	}

#define PROFILE_BEGIN_INTERNAL(variable, tag, id, bytes) PROFILE_BEGIN_INTERNAL_(PROF__VAR(variable), tag, id, bytes)
#define PROFILE_END(variable)                            PROFILE_END_(PROF__VAR(variable))

#define PROFILE_BEGIN_BANDWIDTH(variable, bytes) PROFILE_BEGIN_INTERNAL(variable, STRINGIFY(variable), PROF__ID, bytes)
#define PROFILE_BEGIN(variable)                  PROFILE_BEGIN_INTERNAL(variable, STRINGIFY(variable), PROF__ID, 0)
#define PROFILE_FUNC_BEGIN                       PROFILE_BEGIN_INTERNAL(Func, __FUNCTION__, PROF__ID, 0)
#define PROFILE_FUNC_END                         PROFILE_END(Func)

#else

#define CREATE_PROFILER_TABLE

#define PROFILE_BEGIN_INTERNAL(...) 
#define PROFILE_END(...)

#define PROFILE_BEGIN_BANDWIDTH(...)
#define PROFILE_BEGIN(...)
#define PROFILE_FUNC_BEGIN
#define PROFILE_FUNC_END

#endif

fn_local double tsc_to_s(uint64_t tsc, uint64_t freq)
{
	return ((double)tsc / (double)freq);
}

fn_local double tsc_to_ms(uint64_t tsc, uint64_t freq)
{
	return 1000.0*tsc_to_s(tsc, freq);
}

fn_local void profiler_begin_frame(void)
{
	static uint64_t cpu_freq = 0;

	if (!cpu_freq)
	{
		cpu_freq = os_estimate_cpu_timer_frequency(100);
	}

    copy_array(profiler_slots_read, profiler_slots, profiler_slots_count);

	for (size_t i = 1; i < profiler_slots_count; i++)
	{
		profiler_slot_t *slot = &profiler_slots[i];

		double exclusive_ms = tsc_to_ms(slot->exclusive_tsc, cpu_freq);
		double inclusive_ms = tsc_to_ms(slot->inclusive_tsc, cpu_freq);

		slot->display_exclusive_ms = 0.99*slot->display_exclusive_ms + 0.01*exclusive_ms;
		slot->display_inclusive_ms = 0.99*slot->display_inclusive_ms + 0.01*inclusive_ms;

		slot->hit_count       = 0;
		slot->exclusive_tsc   = 0;
		slot->inclusive_tsc   = 0;
		slot->bytes_processed = 0;
	}
}

fn_local size_t slot_index_from_slot(profiler_slot_t *table, profiler_slot_t *slot)
{
	ASSERT(slot >= table && slot < table + profiler_slots_count);
	return (size_t)(slot - table);
}