// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

fn void *stbi_malloc(size_t size);
fn void *stbi_realloc(void *ptr, size_t new_size);

#define STBI_MALLOC(sz)          stbi_malloc(size)

#define STBI_REALLOC(ptr, newsz) stbi_realloc(ptr, newsz)
#define STBI_FREE(ptr)           (void)ptr

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

global thread_local arena_t *stbi_arena;

void *stbi_malloc(size_t size)
{
    size_t *base = m_alloc_nozero(stbi_arena, size + 8, 8);
    *base = size;

    void *result = base + 1;
    return result;
}

void *stbi_realloc(void *ptr, size_t new_size)
{
    void *result = stbi_malloc(new_size);
    if (ptr)
    {
        size_t old_size = ((size_t *)ptr)[-1];
        copy_memory(result, ptr, old_size);
    }
    return result;
}

//
// streaming asset API
//

typedef struct asset_node_t
{
	struct asset_node_t *previous_version; // re: hot reloading
	void *asset_blob;
} asset_node_t;

typedef struct asset_slot_t
{
	alignas(64) atomic uint32_t state;
	PAD(60);

	// for the time being, every single asset slot gets its own arena:
	arena_t arena;
	// but that's not very sustainable long term...

	mutex_t mutex; // this is only used for sanity checking the threading, it shouldn't ever be contended

	asset_hash_t hash;
	asset_kind_t kind;

	string_storage_t(1024) path;

	// asset_node_t *latest;
	union
	{
		asset_image_t image;
		waveform_t waveform;
	};
} asset_slot_t;

#if 0
// asset_blob is an image_t, or waveform_t, etc depending on asset kind
fn_local void asset__update_slot(asset_slot_t *slot, void *asset_blob)
{
	asset_node_t *node = m_alloc_struct(&slot->arena, asset_node_t);
	node->asset_blob = asset_blob;

	// atomic linked list insert
	asset_node_t *previous_latest = slot->latest;
	do
	{
		node->previous_version = previous_latest;
	}
	while (atomic_cas_ptr((void **)&slot->latest, node, previous_latest) != previous_latest);
}
#endif

asset_image_t missing_image;
waveform_t    missing_waveform;

static arena_t        asset_arena;
static pool_t         asset_store = INIT_POOL(asset_slot_t);
static table_t        asset_index;
static asset_config_t asset_config;
static file_watcher_t asset_watcher;

typedef enum asset_job_kind_t
{
	ASSET_JOB_NONE,

	ASSET_JOB_LOAD_FROM_DISK,

	ASSET_JOB_COUNT,
} asset_job_kind_t;

typedef struct asset_job_t
{
	asset_job_kind_t kind;
	asset_slot_t    *asset;
} asset_job_t;

static void asset_job_proc(job_context_t *context, void *userdata)
{
	(void)context;

	asset_job_t  *job   = userdata;
	asset_slot_t *asset = job->asset;

	bool success = mutex_try_lock(&asset->mutex);
	ASSERT(success); // there should be no contention on assets...

	if (!success)
	{
		mutex_lock(&asset->mutex); // but if there is, don't break stuff if asserts are compiled out! just wait!
	}

	switch (job->kind)
	{
		case ASSET_JOB_LOAD_FROM_DISK:
		{
			asset_state_t state = asset->state;

			if (!(state & AssetState_being_loaded))
				break;

			bool loaded_successfully = true;

			switch (asset->kind)
			{
				case AssetKind_image:
				{
					if (RESOURCE_HANDLE_VALID(asset->image.rhi_texture))
					{
						rhi_destroy_texture(asset->image.rhi_texture);
						NULLIFY_HANDLE(&asset->image.rhi_texture);
					}

					image_t image = load_image_from_disk(&asset->arena, string_from_storage(asset->path), 4);

					asset->image.w         = image.info.w;
					asset->image.h         = image.info.h;
					asset->image.format    = PixelFormat_r8g8b8a8_unorm,
					asset->image.mip_count = 1;
					asset->image.mips[0]   = (image_mip_t){
						.w     = image.info.w,
						.h     = image.info.h,
						.pitch = image.pitch,
					};

					asset->image.rhi_texture = rhi_create_texture(&(rhi_create_texture_params_t){
						.debug_name = string_from_storage(asset->path),
						.dimension  = RhiTextureDimension_2d,
						.width      = image.info.w,
						.height     = image.info.h,
						.depth      = 1,
						.mip_levels = 1,
						.format     = PixelFormat_r8g8b8a8_unorm_srgb,
						.initial_data = &(rhi_texture_data_t){
							.subresources      = &image.pixels,
							.subresource_count = 1,
							.row_stride        = image.pitch,
						},
					});
#if 0
					asset->image = load_image_from_disk(&asset->arena, string_from_storage(asset->path), 4);
					asset->image.renderer_handle = render->upload_texture(&(r_upload_texture_t){
						.upload_flags = R_UPLOAD_TEXTURE_GEN_MIPMAPS,
						.desc = {
							.type   = R_TEXTURE_TYPE_2D,
							.format = R_PIXEL_FORMAT_SRGB8_A8,
							.w      = asset->image.info.w,
							.h      = asset->image.info.h,
						},
						.data = {
							.pitch  = asset->image.pitch,
							.pixels = asset->image.pixels,
						},
					});
#endif
				} break;

				case AssetKind_waveform:
				{
					asset->waveform = load_waveform_from_disk(&asset->arena, string_from_storage(asset->path));
				} break;

				case AssetKind_map:
				{
					INVALID_CODE_PATH; // TODO: handle
				} break;

				default:
				{
					loaded_successfully = false;
                    INVALID_CODE_PATH;
				} break;
			}

			if (loaded_successfully)
			{
				asset->state = (state & ~AssetState_being_loaded) | AssetState_resident;
			}
		} break;

        INVALID_DEFAULT_CASE;
	}

	mutex_unlock(&asset->mutex);
}

static void preload_asset_info(asset_slot_t *asset)
{
	switch (asset->kind)
	{
		case AssetKind_image:
		{
			// image_info_t *info = &asset->image.image.info;
			asset_image_t *image = &asset->image;

			m_scoped_temp
			{
				stbi_arena = temp;

				string_t path = string_from_storage(asset->path);

				int x, y, comp;
				if (stbi_info(string_null_terminate(temp, path).data, &x, &y, &comp))
				{
					image->w             = (uint32_t)x;
					image->h             = (uint32_t)y;
					// image->channel_count = (uint32_t)comp;
				}
			}
		} break;

        case AssetKind_waveform:
        {
            asset->waveform = load_waveform_info_from_disk(string_from_storage(asset->path));
        } break;

		case AssetKind_map:
		{
			// do nothing, for now
		} break;
        
        INVALID_DEFAULT_CASE;
	}

	atomic_fetch_or(&asset->state, AssetState_info_resident);
}

void initialize_asset_system(const asset_config_t *config)
{
    copy_struct(&asset_config, config);

	m_scoped_temp
	{
		for (fs_entry_t *entry = fs_scan_directory(temp, S("gamedata"), FsScanDirectory_recursive);
			 entry;
			 entry = fs_entry_next(entry))
		{
			if (entry->kind == FsEntryKind_directory)
				continue;

			asset_kind_t kind = AssetKind_none;

			string_t ext = string_extension(entry->name);
			if (string_match_nocase(ext, S(".png")) ||
			    string_match_nocase(ext, S(".jpg")) ||
				string_match_nocase(ext, S(".tga")))
			{
				kind = AssetKind_image;
			}
			else if (string_match_nocase(ext, S(".wav")))
			{
				kind = AssetKind_waveform;
			}
			else if (string_match_nocase(ext, S(".map")))
			{
				kind = AssetKind_map;
			}

			if (kind)
			{
				asset_slot_t *asset = pool_add(&asset_store);
				asset->hash  = asset_hash_from_string(entry->path);
				asset->kind  = kind;
				asset->state = AssetState_on_disk;
				string_into_storage(asset->path, entry->path);

				table_insert_object(&asset_index, asset->hash.value, asset);

				preload_asset_info(asset); // stuff like image dimensions we'd like to know right away
			}
		}
	}

	file_watcher_init(&asset_watcher);
	file_watcher_add_directory(&asset_watcher, S("gamedata"));
}

fn_local string_t stringify_flag(string_t total_string, uint32_t flags, uint32_t flag, string_t flag_string)
{
	string_t result = total_string;

	if (flags & flag)
	{
		if (total_string.count)
		{
			result = Sf("%.*s|%.*s", Sx(total_string), Sx(flag_string));
		}
		else
		{
			result = flag_string;
		}
	}

	return result;
}

void process_asset_changes(void)
{
	// TODO: Don't spam reloads right away

	m_scoped_temp
	for (file_event_t *event = file_watcher_get_events(&asset_watcher, temp); 
		 event;
		 event = event->next)
	{
		string_t flags_string = {0};
		flags_string = stringify_flag(flags_string, event->flags, FileEvent_Added, S("Added"));
		flags_string = stringify_flag(flags_string, event->flags, FileEvent_Removed, S("Removed"));
		flags_string = stringify_flag(flags_string, event->flags, FileEvent_Modified, S("Modified"));
		flags_string = stringify_flag(flags_string, event->flags, FileEvent_Renamed, S("Renamed"));

		string_t path = string_normalize_path(temp, event->path);

		log(Asset, Info, "File event for '%.*s': %.*s", Sx(path), Sx(flags_string));

		reload_asset(asset_hash_from_string(path));
	}
}

bool asset_exists(asset_hash_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	return asset && asset->kind == kind;
}

string_t get_asset_path_on_disk(asset_hash_t hash)
{
    string_t result = {0};

	asset_slot_t *asset = table_find_object(&asset_index, hash.value);

    int64_t state = asset->state;
    if (state >= AssetState_on_disk) // not sure this greater than thing is a great(er than) idea
    {
        result = string_from_storage(asset->path);
    }

    return result;
}

static asset_slot_t *get_or_load_asset_async(asset_hash_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	if (asset && asset->kind == kind)
	{
		uint32_t state     = asset->state;
		uint32_t new_state = state | AssetState_being_loaded;

		bool should_load = (state & AssetState_on_disk) && !(state & (AssetState_resident|AssetState_being_loaded));

		if (should_load &&
			atomic_compare_exchange_strong(&asset->state, &state, new_state))
		{
			asset_job_t job = {
				.kind  = ASSET_JOB_LOAD_FROM_DISK,
				.asset = asset,
			};
			add_job_to_queue_with_data(low_priority_job_queue, asset_job_proc, job);
		}
	}
	return asset;
}

static asset_slot_t *get_or_load_asset_blocking(asset_hash_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	if (asset && asset->kind == kind)
	{
		uint32_t state     = asset->state;
		uint32_t new_state = state | AssetState_being_loaded;

		bool should_load = (state & AssetState_on_disk) && !(state & (AssetState_being_loaded));

		if (should_load &&
			atomic_compare_exchange_strong(&asset->state, &state, new_state))
		{
			asset_job_t job = {
				.kind  = ASSET_JOB_LOAD_FROM_DISK,
				.asset = asset,
			};

			job_context_t context = { 0 };
			asset_job_proc(&context, &job);
		}
	}
	return asset;
}

void reload_asset(asset_hash_t hash)
{
	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	if (asset)
	{
		uint32_t state     = asset->state;
		uint32_t new_state = state | AssetState_being_loaded;

		bool should_load = (state & AssetState_on_disk) && !(state & AssetState_being_loaded);

		if (should_load &&
			atomic_compare_exchange_strong(&asset->state, &state, new_state))
		{
			asset_job_t job = {
				.kind  = ASSET_JOB_LOAD_FROM_DISK,
				.asset = asset,
			};
			add_job_to_queue_with_data(low_priority_job_queue, asset_job_proc, job);
		}
	}
}

asset_image_t *get_image(asset_hash_t hash)
{
	asset_image_t *result = &missing_image;

	asset_slot_t *asset = get_or_load_asset_async(hash, AssetKind_image);
	if (asset && (asset->state & AssetState_resident))
	{
		result = &asset->image;
	}

	return result;
}

image_info_t get_image_info(asset_hash_t hash)
{
	image_info_t result = {0};

	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	if (asset && asset->kind == AssetKind_image)
	{
		result.w      = asset->image.w;
		result.h      = asset->image.h;
		result.format = asset->image.format;
	}

	return result;
}


asset_image_t *get_image_blocking(asset_hash_t hash)
{
	asset_image_t *image = &missing_image;

	asset_slot_t *asset = get_or_load_asset_blocking(hash, AssetKind_image);
	if (asset)
	{
		image = &asset->image;
	}

	return image;
}

waveform_t *get_waveform(asset_hash_t hash)
{
	waveform_t *waveform = &missing_waveform;

	asset_slot_t *asset = get_or_load_asset_async(hash, AssetKind_waveform);
	if (asset)
	{
		waveform = &asset->waveform;
	}

	return waveform;
}

waveform_t *get_waveform_blocking(asset_hash_t hash)
{
	waveform_t *waveform = &missing_waveform;

	asset_slot_t *asset = get_or_load_asset_blocking(hash, AssetKind_waveform);
	if (asset)
	{
		waveform = &asset->waveform;
	}

	return waveform;
}

//
//
//

image_t load_image_from_memory(arena_t *arena, string_t path, unsigned nchannels)
{
	(void)arena;
	(void)path;
	(void)nchannels;

	// TODO: Implement
	INVALID_CODE_PATH;

	return (image_t){0};
}

image_t load_image_from_disk(arena_t *arena, string_t path, unsigned nchannels)
{
	image_t result = {0};

	arena_t *temp = m_get_temp(&arena, 1);

	m_scoped(temp)
	{
		stbi_arena = temp;

		int w = 0, h = 0, n = 0;
		unsigned char *pixels = stbi_load(string_null_terminate(temp, path).data, &w, &h, &n, nchannels);

		result = (image_t){
			.info.w = (unsigned)w,
			.info.h = (unsigned)h,
			.pitch  = nchannels*w,
			.pixels = m_copy(arena, pixels, nchannels*w*h),
		};
	}

    return result;
}

bool split_image_into_cubemap_faces(const image_t *source, cubemap_t *cubemap)
{
    /*      ------
     *      | +y |
     * ---------------------
     * | -x | +z | +x | +z |
     * ---------------------
     *      | -y |
     *      ------
     *
     * faces[0] = +x
     * faces[1] = -x
     * faces[2] = +y
     * faces[3] = -y
     * faces[4] = +z
     * faces[5] = -z
     */

    unsigned w = source->info.w / 4;
    unsigned h = source->info.h / 3;

    if (source->info.w % w != 0 ||
        source->info.h % h != 0)
    {
        return false;
    }

    v2i_t face_to_index[] = {
        { 2, 1 }, // +x
        { 0, 1 }, // -x
        { 1, 0 }, // +y
        { 1, 2 }, // -y
        { 1, 1 }, // +z
        { 1, 3 }, // -z
    };

    uint32_t *pixels = source->pixels;

    cubemap->w = w;
    cubemap->h = h;
    cubemap->pitch = source->pitch;

    for (size_t i = 0; i < 6; i++)
    {
        v2i_t index = face_to_index[i];
        int x_offset = w*index.x;
        int y_offset = h*index.y;

        cubemap->pixels[i] = pixels + y_offset*source->pitch + x_offset;
    }

    return true;
}

//
//
//

#define RIFF_ID(string) (((uint32_t)(string)[0] << 0)|((uint32_t)(string)[1] << 8)|((uint32_t)(string)[2] << 16)|((uint32_t)(string)[3] << 24))

typedef struct wave_riff_chunk_t
{
	uint32_t chunk_id;
	uint32_t chunk_size;
	uint32_t format;
} wave_riff_chunk_t;

typedef struct wave_fmt_chunk_t
{
	uint32_t chunk_id;
	uint32_t chunk_size;
	uint16_t audio_format;
	uint16_t channel_count;
	uint32_t sample_rate; 
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
} wave_fmt_chunk_t;

typedef struct wave_data_chunk_t
{
	uint32_t chunk_id;
	uint32_t chunk_size;
	int16_t  data[1];
} wave_data_chunk_t;

#define LOG_ERROR(fmt, ...) log(Asset, Error, fmt, ##__VA_ARGS__)
#define PRINT_RIFF_ID(id) 4, (char *)&id

bool parse_waveform_header_internal(string_t file, 
									wave_riff_chunk_t **out_riff,
									wave_fmt_chunk_t **out_fmt,
									wave_data_chunk_t **out_data)
{
	// TODO: Defer format related error handling to loader functions

	bool result = false;

	if (file.count < sizeof(wave_riff_chunk_t) + sizeof(wave_fmt_chunk_t) + sizeof(wave_data_chunk_t))
	{
		LOG_ERROR("Implausible file size while loading .wav.");
		goto bail; // implausible file size
	}

	char *base = (char *)file.data;

	wave_riff_chunk_t *riff_chunk = (wave_riff_chunk_t *)base;

	if (riff_chunk->chunk_id != RIFF_ID("RIFF"))
	{
		LOG_ERROR("Unexpected chunk ID while loading .wav (expected 'RIFF', got '%.*s')", PRINT_RIFF_ID(riff_chunk->chunk_id));
		goto bail; // unexpected chunk
	}

	if (riff_chunk->format != RIFF_ID("WAVE"))
	{
		LOG_ERROR("Unexpected chunk format while loading .wav (expected 'WAVE', got '%.*s')", PRINT_RIFF_ID(riff_chunk->format));
		goto bail; // wrong format
	}

	wave_fmt_chunk_t *fmt_chunk = (wave_fmt_chunk_t *)(riff_chunk + 1);

	if (fmt_chunk->chunk_id != RIFF_ID("fmt "))
	{
		LOG_ERROR("Unexpected chunk ID while loading .wav (expected 'fmt ', got '%.*s')", PRINT_RIFF_ID(fmt_chunk->chunk_id));
		goto bail; // unexpected chunk
	}

	if (fmt_chunk->audio_format != 1)
	{
		LOG_ERROR("Unsupported audio format while loading .wav (expected 1, got %u)", fmt_chunk->audio_format);
		goto bail; // unexpected format (only want uncompressed PCM)
	}

	if (fmt_chunk->bits_per_sample != 16)
	{
		LOG_ERROR("Unsupported bits per sample while loading .wav (expected 16, got %u)", fmt_chunk->bits_per_sample);
		goto bail; // unexpected bits per sample (I may later allow other bit depths)
	}

	if (fmt_chunk->sample_rate != DREAM_MIX_SAMPLE_RATE)
	{
		LOG_ERROR("Unsupported sample rate while loading .wav (expected %u, got %u)", DREAM_MIX_SAMPLE_RATE, fmt_chunk->sample_rate);
		goto bail;
	}

	wave_data_chunk_t *data_chunk = (wave_data_chunk_t *)(fmt_chunk + 1);

	if (data_chunk->chunk_id != RIFF_ID("data"))
	{
		LOG_ERROR("Unexpected chunk ID while loading .wav (expected 'data', got '%.*s')", PRINT_RIFF_ID(data_chunk->chunk_id));
		goto bail; // unexpected chunk
	}

	*out_riff = riff_chunk;
	*out_fmt  = fmt_chunk;
	*out_data = data_chunk;

	result = true;

bail:
	return result;
}

#undef LOG_ERROR
#undef PRINT_RIFF_ID

waveform_t load_waveform_info_from_memory(string_t file)
{
	waveform_t result = {0};

	wave_riff_chunk_t *riff_chunk;
	wave_fmt_chunk_t  *fmt_chunk;
	wave_data_chunk_t *data_chunk;

	if (parse_waveform_header_internal(file, &riff_chunk, &fmt_chunk, &data_chunk))
	{
		uint32_t sample_count = data_chunk->chunk_size / 2;

		result.channel_count = fmt_chunk->channel_count;
		result.frame_count   = sample_count / result.channel_count;
		result.sample_rate   = fmt_chunk->sample_rate;
	}

	return result;
}

waveform_t load_waveform_info_from_disk(string_t path)
{
    waveform_t result = {0};

    m_scoped_temp
    {
		string_t file = fs_read_entire_file(temp, path);
		result = load_waveform_info_from_memory(file);
    }

    return result;
}

waveform_t load_waveform_from_memory(arena_t *arena, string_t file)
{
	waveform_t result = {0};

	wave_riff_chunk_t *riff_chunk;
	wave_fmt_chunk_t  *fmt_chunk;
	wave_data_chunk_t *data_chunk;

	if (parse_waveform_header_internal(file, &riff_chunk, &fmt_chunk, &data_chunk))
	{
		uint32_t sample_count = data_chunk->chunk_size / 2;

		result.channel_count = fmt_chunk->channel_count;
		result.frame_count   = sample_count / result.channel_count;
		result.sample_rate   = fmt_chunk->sample_rate;

		if (result.channel_count == 1)
		{
			result.frames = m_copy(arena, data_chunk->data, data_chunk->chunk_size);
		}
		else if (result.channel_count == 2)
		{
			result.frames = m_alloc_nozero(arena, data_chunk->chunk_size, 16);

			int16_t *dst_l = waveform_channel(&result, 0);
			int16_t *dst_r = waveform_channel(&result, 1);

			int16_t *src = data_chunk->data;
			for (size_t frame_index = 0; frame_index < result.frame_count; frame_index++)
			{
				*dst_l++ = *src++;
				*dst_r++ = *src++;
			}
		}
		else
		{
			ASSERT(result.channel_count > 2);

			result.frames = m_alloc_nozero(arena, data_chunk->chunk_size, 16);

			int16_t *src = data_chunk->data;
			for (size_t frame_index = 0; frame_index < result.frame_count; frame_index++)
			{
				for (size_t channel_index = 0; channel_index < result.channel_count; channel_index++)
				{
					int16_t *dst = waveform_channel(&result, channel_index);
					dst[frame_index] = *src++;
				}
			}
		}
	}

	return result;
}

waveform_t load_waveform_from_disk(arena_t *arena, string_t path)
{
	waveform_t result = {0};

	arena_t *temp = m_get_temp(&arena, 1);

	m_scoped(temp)
	{
		string_t file = fs_read_entire_file(temp, path);
		result = load_waveform_from_memory(arena, file);
	}

	return result;
}
