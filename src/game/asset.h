// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#define DREAM_MIX_SAMPLE_RATE 44100

//
// streaming asset API
//

typedef enum asset_kind_t
{
	AssetKind_none,

	AssetKind_image,
	AssetKind_waveform,
	AssetKind_map,

	AssetKind_count,
} asset_kind_t;

typedef struct image_info_t
{
	uint32_t w;
	uint32_t h;
	pixel_format_t format;
} image_info_t;

typedef struct image_t
{
	image_info_t info;

	uint32_t pitch;
	void    *pixels;
} image_t;

typedef struct image_mip_t
{
	uint32_t w;
	uint32_t h;
	uint32_t pitch;
	void    *pixels;
} image_mip_t;

typedef struct asset_image_t
{
	uint32_t w;
	uint32_t h;

	pixel_format_t format;

	uint32_t    mip_count;
	image_mip_t mips[16];

	rhi_texture_t     rhi_texture;
//	rhi_texture_srv_t rhi_texture_srv;
} asset_image_t;

typedef enum asset_state_t
{
	AssetState_none = 0,

	AssetState_on_disk       = 1 << 0,
	AssetState_being_loaded  = 1 << 1,
	AssetState_info_resident = 1 << 2,
	AssetState_resident      = 1 << 3,

	AssetState_count,
} asset_state_t;

typedef struct asset_hash_t
{
	uint64_t value;
} asset_hash_t;

fn_local asset_hash_t asset_hash_from_string(string_t string)
{
	asset_hash_t result = {
		.value = string_hash(string),
	};
	return result;
}

typedef struct cubemap_t
{
	uint32_t w;
	uint32_t h;
	uint32_t pitch;
	uint32_t channel_count;
	void    *pixels[6];

	texture_handle_t renderer_handle;
} cubemap_t;

typedef struct waveform_t
{
	uint32_t channel_count;
    uint32_t sample_rate;
	size_t   frame_count;
	int16_t *frames;
} waveform_t;

fn_local int16_t *waveform_channel(const waveform_t *waveform, size_t index)
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

typedef struct asset_system_t
{
	asset_image_t  missing_image;
	waveform_t     missing_waveform;

	arena_t        arena;
	pool_t         asset_store;
	table_t        asset_index;
	asset_config_t asset_config;
	file_watcher_t asset_watcher;
} asset_system_t;

thread_local asset_system_t *g_assets;

fn void asset_system_equip  (asset_system_t *assets);
fn void asset_system_unequip(void);
fn asset_system_t *asset_system_get(void);

fn asset_system_t *asset_system_make(void);

fn void process_asset_changes(void);

// fn from here on out because d3d11.c is using some of these.
// Should I fix that? Yes I think so, I imagine the renderer should be
// modular and really shouldn't have a (direct) dependency on this specific
// game's asset system.

fn asset_image_t missing_image;
fn waveform_t    missing_waveform;

fn bool        asset_exists          (asset_hash_t hash, asset_kind_t kind);
fn string_t    get_asset_path_on_disk(asset_hash_t hash); 
fn void        reload_asset          (asset_hash_t hash);

fn asset_image_t *get_image            (asset_hash_t hash);
fn image_info_t   get_image_info       (asset_hash_t hash);
fn image_t       *get_missing_image    (void);
fn waveform_t    *get_waveform         (asset_hash_t hash);
fn waveform_t    *get_missing_waveform (void);

fn asset_image_t *get_image_blocking   (asset_hash_t hash);
fn waveform_t    *get_waveform_blocking(asset_hash_t hash);

fn_local asset_image_t *get_image_from_string(string_t string)
{
	return get_image(asset_hash_from_string(string));
}

fn_local waveform_t *get_waveform_from_string(string_t string)
{
	return get_waveform(asset_hash_from_string(string));
}

fn_local waveform_t *get_waveform_from_string_blocking(string_t string)
{
	return get_waveform_blocking(asset_hash_from_string(string));
}

//
// raw asset loading
//

fn image_t load_image_from_memory(arena_t *arena, string_t file_data, unsigned nchannels);
fn image_t load_image_from_disk  (arena_t *arena, string_t path, unsigned nchannels);

fn bool split_image_into_cubemap(const image_t *source, cubemap_t *cubemap);

fn waveform_t load_waveform_info_from_memory(string_t file_data);
fn waveform_t load_waveform_info_from_disk  (string_t path);
fn waveform_t load_waveform_from_memory(arena_t *arena, string_t file_data);
fn waveform_t load_waveform_from_disk  (arena_t *arena, string_t path);
