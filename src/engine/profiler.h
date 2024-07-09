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
	const char *tag;           // 8
	uint64_t hit_count;        // 16
	uint64_t exclusive_tsc;    // 24
	uint64_t inclusive_tsc;    // 32
	uint64_t bytes_processed;  // 40
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

#define CREATE_PROFILER_TABLE                             \
    profiler_slot_t profiler_slots     [__COUNTER__ + 1]; \
    profiler_slot_t profiler_slots_read[__COUNTER__];     \
    uint64_t profiler_slots_count = __COUNTER__ - 1;

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
#define PROFILE_BEGIN_FUNC                       PROFILE_BEGIN_INTERNAL(Func, __FUNCTION__, PROF__ID, 0)
#define PROFILE_END_FUNC                         PROFILE_END(Func)

#else

#define CREATE_PROFILER_TABLE

#define PROFILE_BEGIN_INTERNAL(...) 
#define PROFILE_END(...)

#define PROFILE_BEGIN_BANDWIDTH(...)
#define PROFILE_BEGIN(...)
#define PROFILE_BEGIN_FUNC
#define PROFILE_END_FUNC

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
    copy_array(profiler_slots_read, profiler_slots, profiler_slots_count);
	zero_array(profiler_slots, profiler_slots_count);
}
