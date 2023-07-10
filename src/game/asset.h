#ifndef ASSET_H
#define ASSET_H

#include "core/api_types.h"
#include "core/string.h"

//
// streaming asset API
//

typedef enum asset_kind_t
{
	ASSET_KIND_NONE,

	ASSET_KIND_IMAGE,
	ASSET_KIND_WAVEFORM,

	ASSET_KIND_COUNT,
} asset_kind_t;

typedef enum asset_state_t
{
	ASSET_STATE_NONE,

	ASSET_STATE_ON_DISK,
	ASSET_STATE_BEING_LOADED_ASYNC,
	ASSET_STATE_IN_MEMORY,

	ASSET_STATE_COUNT,
} asset_state_t;

typedef struct asset_hash_t
{
	uint64_t value;
} asset_hash_t;

DREAM_INLINE asset_hash_t asset_hash_from_string(string_t string)
{
	asset_hash_t result = {
		.value = string_hash(string),
	};
	return result;
}

typedef struct image_t
{
	uint32_t w;
	uint32_t h;
	uint32_t pitch;
	uint32_t channel_count;
	void    *pixels;

	resource_handle_t gpu;
} image_t;

typedef struct cubemap_t
{
	uint32_t w;
	uint32_t h;
	uint32_t pitch;
	uint32_t channel_count;
	void    *pixels[6];

	resource_handle_t gpu;
} cubemap_t;

typedef struct waveform_t
{
	uint32_t channel_count;
    uint32_t sample_rate;
	size_t   frame_count;
	int16_t *frames;
} waveform_t;

DREAM_INLINE int16_t *waveform_channel(const waveform_t *waveform, size_t index)
{
    int16_t *result = NULL;

    if (index < waveform->channel_count)
    {
        result = waveform->frames + waveform->frame_count*index;
    }

    return result;
}

typedef struct asset_config_t
{
    uint32_t mix_sample_rate;
} asset_config_t;

DREAM_API void initialize_asset_system(const asset_config_t *config);
DREAM_API void preload_asset(asset_hash_t hash);

DREAM_API image_t    missing_image;
DREAM_API waveform_t missing_waveform;

DREAM_API bool        asset_exists         (asset_hash_t hash, asset_kind_t kind);

DREAM_API image_t    *get_image            (asset_hash_t hash);
DREAM_API image_t    *get_missing_image    (void);
DREAM_API waveform_t *get_waveform         (asset_hash_t hash);
DREAM_API image_t    *get_missing_waveform (void);

DREAM_API image_t    *get_image_blocking   (asset_hash_t hash);
DREAM_API waveform_t *get_waveform_blocking(asset_hash_t hash);

DREAM_INLINE image_t *get_image_from_string(string_t string)
{
	return get_image(asset_hash_from_string(string));
}

DREAM_INLINE waveform_t *get_waveform_from_string(string_t string)
{
	return get_waveform(asset_hash_from_string(string));
}

DREAM_INLINE waveform_t *get_waveform_from_string_blocking(string_t string)
{
	return get_waveform_blocking(asset_hash_from_string(string));
}

//
// raw asset loading
//

DREAM_API image_t load_image_from_memory(arena_t *arena, string_t file_data, unsigned nchannels);
DREAM_API image_t load_image_from_disk  (arena_t *arena, string_t path, unsigned nchannels);

DREAM_API bool split_image_into_cubemap(const image_t *source, cubemap_t *cubemap);

#define WAVE_SAMPLE_RATE 44100

DREAM_API waveform_t load_waveform_info_from_memory(string_t file_data);
DREAM_API waveform_t load_waveform_info_from_disk  (string_t path);
DREAM_API waveform_t load_waveform_from_memory(arena_t *arena, string_t file_data);
DREAM_API waveform_t load_waveform_from_disk  (arena_t *arena, string_t path);

#endif /* ASSET_H */
