// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct sort_key_t
{
	uint32_t index;
	uint32_t key;
} sort_key_t;

fn void radix_sort_u32 (uint32_t   *array, size_t size);
fn void radix_sort_u64 (uint64_t   *array, size_t size);
fn void radix_sort_keys(sort_key_t *array, size_t size);

typedef int (*comparison_function_t)(const void *l, const void *r, void *user_data);

fn void merge_sort(void *array, size_t count, size_t element_size, comparison_function_t func, void *user_data);

#define merge_sort_array(array, count, func, user_data) \
	merge_sort(array, count, sizeof((array)[0]), func, user_data)
