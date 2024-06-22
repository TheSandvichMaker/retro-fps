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
