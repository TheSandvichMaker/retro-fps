#ifndef ASSET_H
#define ASSET_H

#include "core/api_types.h"

typedef struct image_t
{
    uint32_t w, h, pitch, nchannels;
    void *pixels;
} image_t;

typedef struct cubemap_t
{
    uint32_t w, h, pitch;
    uint32_t *pixels[6];
} cubemap_t;

image_t load_image(arena_t *arena, string_t path, unsigned nchannels);
bool split_image_into_cubemap(const image_t *source, cubemap_t *cubemap);

#define WAVE_SAMPLE_RATE 44100

typedef struct waveform_t
{
	uint32_t channel_count;
	uint32_t frame_count; // sample_count / channel_count
	int16_t *frames;
} waveform_t;

waveform_t load_waveform_from_memory(arena_t *arena, string_t file_data);
waveform_t load_waveform_from_disk(arena_t *arena, string_t path);

#endif /* ASSET_H */
