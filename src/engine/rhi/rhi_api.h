// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct rhi_init_params_t
{
	uint32_t frame_buffer_count;
} rhi_init_params_t;

typedef enum rhi_backend_t
{
	RhiBackend_none,

	RhiBackend_d3d12,

	RhiBackend_count,
} rhi_backend_t;

DEFINE_HANDLE_TYPE(rhi_window_t);
