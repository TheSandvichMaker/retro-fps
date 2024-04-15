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

#define CREATE_PROFILER_TABLE profiler_slot_t profiler_slots[__COUNTER__ + 1]; uint64_t profiler_slots_count = __COUNTER__;

#define PROF__VAR(var) CONCAT(Prof__, var)
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

fn_local void init_profiler(void)
{
	profiler.start_tsc = read_cpu_timer();
}

fn_local double tsc_to_s(uint64_t tsc, uint64_t freq)
{
	return ((double)tsc / (double)freq);
}

fn_local double tsc_to_ms(uint64_t tsc, uint64_t freq)
{
	return 1000.0*tsc_to_s(tsc, freq);
}

#if 0
static void print_profiler_stats(void)
{
	profiler.end_tsc = read_cpu_timer();

	uint64_t cpu_freq = estimate_cpu_timer_frequency(250);
	double total_time_ms = tsc_to_ms(profiler.end_tsc - profiler.start_tsc, cpu_freq);

	fprintf(stderr, "Estimated CPU frequency: %llu\n", cpu_freq);
	fprintf(stderr, "Total runtime: %.3fms\n", total_time_ms);

#if PROFILER
	if (profiler_slots_count > 1)
	{
		uint64_t *sorted_slot_indices = malloc(sizeof(uint64_t)*profiler_slots_count);
		for (size_t i = 1; i < profiler_slots_count; i += 1)
		{
			sorted_slot_indices[i - 1] = i;
		}

		qsort(sorted_slot_indices, profiler_slots_count - 1, sizeof(uint64_t), compare_profile_slot_descending);

		double total_pct = 0.0;

		printf("Profiler region timings:\n");
		for (size_t j = 0; j < profiler_slots_count - 1; j += 1)
		{
			size_t i = sorted_slot_indices[j];

			profiler_slot_t *slot = &profiler_slots[i];
			double exclusive_ms  = tsc_to_ms(slot->exclusive_tsc, cpu_freq);
			double inclusive_ms  = tsc_to_ms(slot->inclusive_tsc, cpu_freq);
			double exclusive_pct = 100.0*(exclusive_ms / total_time_ms);
			double inclusive_pct = 100.0*(inclusive_ms / total_time_ms);
			printf("  %-24s %.2f/%.2fms (%.2f/%.2f%%, %zu hits", slot->tag, exclusive_ms, inclusive_ms, exclusive_pct, inclusive_pct, slot->hit_count);

			if (slot->bytes_processed > 0)
			{
				double megabytes_processed  = (double)slot->bytes_processed / (1024.0*1024.0);
				double gigabytes_per_second = (megabytes_processed / 1024.0) / tsc_to_s(slot->exclusive_tsc, cpu_freq);
				printf(", %.2fMiB at %.2fGiB/s", megabytes_processed, gigabytes_per_second);
			}

			printf(")\n");

			total_pct += exclusive_pct;
		}
		printf("Sum of percentages: %.2f%%\n", total_pct);
	}
#endif
}
#endif
