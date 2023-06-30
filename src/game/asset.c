#include "asset.h"

static void *stbi_malloc(size_t size);
static void *stbi_realloc(void *ptr, size_t new_size);

#define STBI_MALLOC(sz)          stbi_malloc(size)
#define STBI_REALLOC(ptr, newsz) stbi_realloc(ptr, newsz)
#define STBI_FREE(ptr)           (void)ptr

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//
//
//

#include "core/common.h"
#include "core/arena.h"
#include "core/fs.h"
#include "core/bulk_data.h"
#include "core/hashtable.h"

#include "game/job_queues.h"

#include "render/render.h"

//
// stbi support
//

static thread_local arena_t *stbi_arena;

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

typedef struct asset_slot_t
{
	asset_hash_t hash;
	asset_kind_t kind;
	int64_t state;

	STRING_STORAGE(256) path;

	union
	{
		image_t    image;
		waveform_t waveform;
	};
} asset_slot_t;

image_t    missing_image;
waveform_t missing_waveform;

static arena_t asset_arena;
static bulk_t  asset_store = INIT_BULK_DATA(asset_slot_t);
static hash_t  asset_index;

typedef enum asset_job_kind_t
{
	ASSET_JOB_NONE,

	ASSET_JOB_LOAD_FROM_DISK,

	ASSET_JOB_COUNT,
} asset_job_kind_t;

typedef struct asset_job_t
{
	asset_job_kind_t kind;
	asset_slot_t *asset;
} asset_job_t;

static void asset_job(job_context_t *context, void *userdata)
{
	(void)context;

	asset_job_t  *job   = userdata;
	asset_slot_t *asset = job->asset;

	switch (job->kind)
	{
		case ASSET_JOB_LOAD_FROM_DISK:
		{
			asset_state_t state = asset->state;

			if (state != ASSET_STATE_BEING_LOADED_ASYNC)
				break;

			bool loaded_successfully = true;

			switch (asset->kind)
			{
				case ASSET_KIND_IMAGE:
				{
					resource_handle_t idiot_code = asset->image.gpu;

					// FIXME: figure out about allocation. temp is thread local so that's cool but that's obviously the wrong way to do this!!
					asset->image = load_image_from_disk(temp, STRING_FROM_STORAGE(asset->path), 4);
					asset->image.gpu = idiot_code;

					render->populate_texture(asset->image.gpu, &(upload_texture_t){
						.upload_flags = UPLOAD_TEXTURE_GEN_MIPMAPS,
						.desc = {
							.type   = TEXTURE_TYPE_2D,
							.format = PIXEL_FORMAT_SRGB8_A8,
							.w      = asset->image.w,
							.h      = asset->image.h,
						},
						.data = {
							.pitch  = asset->image.pitch,
							.pixels = asset->image.pixels,
						},
					});
				} break;

				case ASSET_KIND_WAVEFORM:
				{
					// FIXME: figure out about allocation. temp is thread local so that's cool but that's obviously the wrong way to do this!!
					asset->waveform = load_waveform_from_disk(temp, STRING_FROM_STORAGE(asset->path));
				} break;

				default:
				{
					loaded_successfully = false;
				} break;
			}

			if (loaded_successfully)
			{
				// x64: writes are atomic (right?)
				asset->state = ASSET_STATE_IN_MEMORY;
			}
		} break;
	}
}

static void preload_asset_info(asset_slot_t *asset)
{
	switch (asset->kind)
	{
		case ASSET_KIND_IMAGE:
		{
			image_t *image = &asset->image;

			stbi_arena = temp;

			m_scoped(temp)
			{
				string_t path = STRING_FROM_STORAGE(asset->path);

				int x, y, comp;
				if (stbi_info(string_null_terminate(temp, path), &x, &y, &comp))
				{
					image->w             = (uint32_t)x;
					image->h             = (uint32_t)y;
					image->channel_count = (uint32_t)comp;
				}
			}

			image->gpu = render->reserve_texture();
		} break;

		// TODO: others
	}
}

void initialize_asset_system(void)
{
	m_scoped(temp)
	{
		for (fs_entry_t *entry = fs_scan_directory(temp, strlit("gamedata"), FS_SCAN_RECURSIVE);
			 entry;
			 entry = fs_entry_next(entry))
		{
			if (entry->kind == FS_ENTRY_DIRECTORY)
				continue;

			asset_kind_t kind = ASSET_KIND_NONE;

			string_t ext = string_extension(entry->name);
			if (string_match_nocase(ext, strlit(".png")) ||
			    string_match_nocase(ext, strlit(".jpg")) ||
				string_match_nocase(ext, strlit(".tga")))
			{
				kind = ASSET_KIND_IMAGE;
			}
			else if (string_match_nocase(ext, strlit(".wav")))
			{
				kind = ASSET_KIND_WAVEFORM;
			}

			if (kind)
			{
				asset_slot_t *asset = bd_add(&asset_store);
				asset->hash  = asset_hash_from_string(entry->path);
				asset->kind  = kind;
				asset->state = ASSET_STATE_ON_DISK;
				STRING_INTO_STORAGE(asset->path, entry->path);

				hash_add_object(&asset_index, asset->hash.value, asset);

				preload_asset_info(asset); // stuff like image dimensions we'd like to know right away
			}
		}
	}
}

bool asset_exists(asset_hash_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = hash_find_object(&asset_index, hash.value);
	return asset && asset->kind == kind;
}

static asset_slot_t *get_or_load_asset_async(asset_hash_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = hash_find_object(&asset_index, hash.value);
	if (asset && asset->kind == kind)
	{
		int64_t state = asset->state;

		if (state == ASSET_STATE_ON_DISK &&
			atomic_cas64(&asset->state, ASSET_STATE_BEING_LOADED_ASYNC, ASSET_STATE_ON_DISK) == ASSET_STATE_ON_DISK)
		{
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			asset_job_t *job = m_alloc_struct(&asset_arena, asset_job_t);
			job->kind  = ASSET_JOB_LOAD_FROM_DISK;
			job->asset = asset,

			add_job_to_queue(high_priority_job_queue, asset_job, job);
		}
	}
	return asset;
}

static asset_slot_t *get_or_load_asset_blocking(asset_hash_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = hash_find_object(&asset_index, hash.value);
	if (asset && asset->kind == kind)
	{
		if (asset->state == ASSET_STATE_ON_DISK)
		{
			asset_job_t job = {
				.kind  = ASSET_JOB_LOAD_FROM_DISK,
				.asset = asset,
			};

			asset->state = ASSET_STATE_BEING_LOADED_ASYNC;

			job_context_t context = { 0 };
			asset_job(&context, &job);
		}
	}
	return asset;
}

image_t *get_image(asset_hash_t hash)
{
	image_t *image = &missing_image;

	asset_slot_t *asset = get_or_load_asset_async(hash, ASSET_KIND_IMAGE);
	// asset_slot_t *asset = get_or_load_asset_blocking(hash, ASSET_KIND_IMAGE);
	if (asset)
	{
		// Always returns the image. Even if it's not resident. You'd know by seeing if pixels is null.
		image = &asset->image;
	}

	return image;
}

image_t *get_image_blocking(asset_hash_t hash)
{
	image_t *image = &missing_image;

	asset_slot_t *asset = get_or_load_asset_blocking(hash, ASSET_KIND_IMAGE);
	if (asset)
	{
		// Always returns the image. Even if it's not resident. You'd know by seeing if pixels is null.
		image = &asset->image;
	}

	return image;
}

waveform_t *get_waveform(asset_hash_t hash)
{
	waveform_t *waveform = &missing_waveform;

	asset_slot_t *asset = hash_find_object(&asset_index, hash.value);
	if (asset && asset->kind == ASSET_KIND_WAVEFORM)
	{
		if (asset->state == ASSET_STATE_IN_MEMORY)
		{
			waveform = &asset->waveform;
		}
		else if (asset->state == ASSET_STATE_ON_DISK)
		{
			// TODO: asynchronous
			asset->waveform = load_waveform_from_disk(&asset_arena, STRING_FROM_STORAGE(asset->path));
			asset->state = ASSET_STATE_IN_MEMORY;
			waveform = &asset->waveform;
		}
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
    stbi_arena = arena;

    int w = 0, h = 0, n = 0;
    unsigned char *pixels = stbi_load(string_null_terminate(temp, path), &w, &h, &n, nchannels);

    image_t result = {
        .w      = (unsigned)w,
        .h      = (unsigned)h,
        .pitch  = sizeof(uint32_t)*w,
        .pixels = pixels,
    };

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

    unsigned w = source->w / 4;
    unsigned h = source->h / 3;

    if (source->w % w != 0 ||
        source->h % h != 0)
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
	int16_t  data[];
} wave_data_chunk_t;

waveform_t load_waveform_from_memory(arena_t *arena, string_t file)
{
	waveform_t result = {0};

	if (file.count < sizeof(wave_riff_chunk_t) + sizeof(wave_fmt_chunk_t) + sizeof(wave_data_chunk_t))
		goto bail; // implausible file size

	char *base = (char *)file.data;

	wave_riff_chunk_t *riff_chunk = (wave_riff_chunk_t *)base;

	if (riff_chunk->chunk_id != RIFF_ID("RIFF"))
		goto bail; // unexpected chunk

	if (riff_chunk->format != RIFF_ID("WAVE"))
		goto bail; // wrong format

	wave_fmt_chunk_t *fmt_chunk = (wave_fmt_chunk_t *)(riff_chunk + 1);

	if (fmt_chunk->chunk_id != RIFF_ID("fmt "))
		goto bail; // unexpected chunk

	if (fmt_chunk->audio_format != 1)
		goto bail; // unexpected format (only want uncompressed PCM)

	if (fmt_chunk->sample_rate != WAVE_SAMPLE_RATE)
		goto bail; // unexpected sample rate (I don't want to do sample rate conversion)

	if (fmt_chunk->bits_per_sample != 16)
		goto bail; // unexpected bits per sample (I may later allow other bit depths)

	wave_data_chunk_t *data_chunk = (wave_data_chunk_t *)(fmt_chunk + 1);

	if (data_chunk->chunk_id != RIFF_ID("data"))
		goto bail; // unexpected chunk

	uint32_t sample_count = data_chunk->chunk_size / 2;

	result.channel_count = fmt_chunk->channel_count;
	result.frame_count   = sample_count / result.channel_count;
	result.frames        = m_copy(arena, data_chunk->data, data_chunk->chunk_size);

bail:

	return result;
}

waveform_t load_waveform_from_disk(arena_t *arena, string_t path)
{
	waveform_t result = {0};

	m_scoped(temp)
	{
		string_t file = fs_read_entire_file(temp, path);
		result = load_waveform_from_memory(arena, file);
	}

	return result;
}
