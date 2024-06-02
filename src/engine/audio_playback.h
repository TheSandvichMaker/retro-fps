// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct platform_audio_io_t platform_audio_io_t;

typedef void (*audio_output_callback_t)(platform_audio_io_t *io);
fn void start_audio_thread(audio_output_callback_t callback);
