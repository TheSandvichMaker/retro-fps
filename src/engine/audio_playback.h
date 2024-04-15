// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef void (*audio_output_callback_t)(size_t frame_count, float *frames);
fn void start_audio_thread(audio_output_callback_t callback);
