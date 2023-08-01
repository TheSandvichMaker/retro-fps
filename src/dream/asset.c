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
#include "core/pool.h"
#include "core/hashtable.h"

#include "dream/job_queues.h"
#include "dream/render.h"

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
	alignas(64) uint32_t state;
	PAD(60);

	asset_table_t hash;
	asset_kind_t kind;

	string_storage_t(256) path;

	union
	{
		image_t    image;
		waveform_t waveform;
	};
} asset_slot_t;

image_t    missing_image;
waveform_t missing_waveform;

static arena_t asset_arena;
static pool_t  asset_store = INIT_POOL(asset_slot_t);
static table_t  asset_index;
static asset_config_t asset_config;

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
	arena_t *arena; // TODO: figure it out.
} asset_job_t;

DREAM_INLINE size_t convert_sample_count(size_t src_sample_rate, size_t dst_sample_rate, size_t sample_count)
{
    // TODO: Overflow check
    size_t result = (sample_count*dst_sample_rate + src_sample_rate - 1) / src_sample_rate;
    return result;
}

static void asset_job(job_context_t *context, void *userdata)
{
	(void)context;

	asset_job_t  *job   = userdata;
	asset_slot_t *asset = job->asset;
	arena_t      *arena = job->arena;

	// FIXME: figure out about allocation. temp is thread local so that's cool but that's obviously the wrong way to do this!!
	if (!arena)
		arena = temp;

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

					asset->image = load_image_from_disk(arena, string_from_storage(asset->path), 4);
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
                    waveform_t  src_waveform = load_waveform_from_disk(temp, string_from_storage(asset->path));
                    waveform_t *dst_waveform = &asset->waveform;

                    ASSERT(src_waveform.channel_count == asset->waveform.channel_count);

                    if (src_waveform.sample_rate == dst_waveform->sample_rate)
                    {
                        // FIXME: FOOLISH CODE FOR DUMB PEOPLE
                        dst_waveform->frames = m_copy_array(arena, src_waveform.frames, src_waveform.channel_count*src_waveform.frame_count);
                    }
                    else
                    {
                        uint32_t channel_count   = src_waveform.channel_count;
                        size_t   src_frame_count = src_waveform.frame_count;
                        size_t   dst_frame_count = dst_waveform->frame_count;

                        int16_t *dst_frames = m_alloc_array_nozero(arena, channel_count*dst_frame_count, int16_t);

                        for (size_t channel_index = 0; channel_index < channel_count; channel_index++)
                        {
                            int16_t *src = waveform_channel(&src_waveform, channel_index);
                            int16_t *dst = dst_frames + channel_index*dst_frame_count;

                            for (size_t dst_frame_index = 0; dst_frame_index < dst_frame_count; dst_frame_index++)
                            {
                                float t = (float)dst_frame_index / (float)dst_frame_count;
                                float x = t*(float)src_frame_count;
                                size_t x_i = (size_t)x;
                                float x_f = (float)(x - (float)x_i);

                                if (x_i >= src_frame_count) 
                                    x_i = src_frame_count - 1;

                                float s_1 = unorm_from_i16(src[x_i]);
                                float s_2 = unorm_from_i16(src[x_i + 1]);
                                float s = lerp(s_1, s_2, x_f);

                                *dst++ = i16_from_unorm(s);
                            }
                        }

                        dst_waveform->frames = dst_frames;
                    }
				} break;

				default:
				{
					loaded_successfully = false;
                    INVALID_CODE_PATH;
				} break;
			}

			if (loaded_successfully)
			{
				asset->state = ASSET_STATE_IN_MEMORY;
			}
		} break;

        INVALID_DEFAULT_CASE;
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
				string_t path = string_from_storage(asset->path);

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

        case ASSET_KIND_WAVEFORM:
        {
            waveform_t waveform = load_waveform_info_from_disk(string_from_storage(asset->path));

            // we're gonna resample waveforms that don't match:
            waveform.frame_count = convert_sample_count(waveform.sample_rate, asset_config.mix_sample_rate, waveform.frame_count);
            waveform.sample_rate = asset_config.mix_sample_rate;

            asset->waveform = waveform;
        } break;
        
        INVALID_DEFAULT_CASE;
	}
}

void initialize_asset_system(const asset_config_t *config)
{
    copy_struct(&asset_config, config);

	m_scoped(temp)
	{
		for (fs_entry_t *entry = fs_scan_directory(temp, S("gamedata"), FS_SCAN_RECURSIVE);
			 entry;
			 entry = fs_entry_next(entry))
		{
			if (entry->kind == FS_ENTRY_DIRECTORY)
				continue;

			asset_kind_t kind = ASSET_KIND_NONE;

			string_t ext = string_extension(entry->name);
			if (string_match_nocase(ext, S(".png")) ||
			    string_match_nocase(ext, S(".jpg")) ||
				string_match_nocase(ext, S(".tga")))
			{
				kind = ASSET_KIND_IMAGE;
			}
			else if (string_match_nocase(ext, S(".wav")))
			{
				kind = ASSET_KIND_WAVEFORM;
			}

			if (kind)
			{
				asset_slot_t *asset = pool_add(&asset_store);
				asset->hash  = asset_hash_from_string(entry->path);
				asset->kind  = kind;
				asset->state = ASSET_STATE_ON_DISK;
				string_into_storage(asset->path, entry->path);

				table_insert_object(&asset_index, asset->hash.value, asset);

				preload_asset_info(asset); // stuff like image dimensions we'd like to know right away
			}
		}
	}
}

bool asset_exists(asset_table_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	return asset && asset->kind == kind;
}

static asset_slot_t *get_or_load_asset_async(asset_table_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	if (asset && asset->kind == kind)
	{
		int64_t state = asset->state;

		if (state == ASSET_STATE_ON_DISK &&
			atomic_cas_u32(&asset->state, ASSET_STATE_BEING_LOADED_ASYNC, ASSET_STATE_ON_DISK) == ASSET_STATE_ON_DISK)
		{
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			// FIXME: FIX LEAK!!!!!
			asset_job_t *job = m_alloc_struct(&asset_arena, asset_job_t);
			job->kind  = ASSET_JOB_LOAD_FROM_DISK;
			job->asset = asset;

			add_job_to_queue(high_priority_job_queue, asset_job, job);
		}
	}
	return asset;
}

static asset_slot_t *get_or_load_asset_blocking(asset_table_t hash, asset_kind_t kind)
{
	asset_slot_t *asset = table_find_object(&asset_index, hash.value);
	if (asset && asset->kind == kind)
	{
		if (asset->state == ASSET_STATE_ON_DISK)
		{
			asset_job_t job = {
				.kind  = ASSET_JOB_LOAD_FROM_DISK,
				.asset = asset,
				.arena = &asset_arena,
			};

			asset->state = ASSET_STATE_BEING_LOADED_ASYNC;

			job_context_t context = { 0 };
			asset_job(&context, &job);
		}
	}
	return asset;
}

image_t *get_image(asset_table_t hash)
{
	image_t *image = &missing_image;

	asset_slot_t *asset = get_or_load_asset_async(hash, ASSET_KIND_IMAGE);
	if (asset)
	{
		image = &asset->image;
	}

	return image;
}

image_t *get_image_blocking(asset_table_t hash)
{
	image_t *image = &missing_image;

	asset_slot_t *asset = get_or_load_asset_blocking(hash, ASSET_KIND_IMAGE);
	if (asset)
	{
		image = &asset->image;
	}

	return image;
}

waveform_t *get_waveform(asset_table_t hash)
{
	waveform_t *waveform = &missing_waveform;

	asset_slot_t *asset = get_or_load_asset_async(hash, ASSET_KIND_WAVEFORM);
	if (asset)
	{
		waveform = &asset->waveform;
	}

	return waveform;
}

waveform_t *get_waveform_blocking(asset_table_t hash)
{
	waveform_t *waveform = &missing_waveform;

	asset_slot_t *asset = get_or_load_asset_blocking(hash, ASSET_KIND_WAVEFORM);
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

waveform_t load_waveform_info_from_memory(string_t file)
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

	if (fmt_chunk->bits_per_sample != 16)
		goto bail; // unexpected bits per sample (I may later allow other bit depths)

	wave_data_chunk_t *data_chunk = (wave_data_chunk_t *)(fmt_chunk + 1);

	if (data_chunk->chunk_id != RIFF_ID("data"))
		goto bail; // unexpected chunk

	uint32_t sample_count = data_chunk->chunk_size / 2;

	result.channel_count = fmt_chunk->channel_count;
    result.frame_count   = sample_count / result.channel_count;
    result.sample_rate   = fmt_chunk->sample_rate;

bail:
	return result;
}

waveform_t load_waveform_info_from_disk(string_t path)
{
    waveform_t result = {0};

    m_scoped(temp)
    {
		string_t file = fs_read_entire_file(temp, path);
		result = load_waveform_info_from_memory(file);
    }

    return result;
}

waveform_t load_waveform_from_memory(arena_t *arena, string_t file)
{
    // TODO: Deduplicate with load_waveform_info

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

	if (fmt_chunk->bits_per_sample != 16)
		goto bail; // unexpected bits per sample (I may later allow other bit depths)

	wave_data_chunk_t *data_chunk = (wave_data_chunk_t *)(fmt_chunk + 1);

	if (data_chunk->chunk_id != RIFF_ID("data"))
		goto bail; // unexpected chunk

	uint32_t sample_count = data_chunk->chunk_size / 2;

	result.channel_count = fmt_chunk->channel_count;
    result.sample_rate   = fmt_chunk->sample_rate;
	result.frame_count   = sample_count / result.channel_count;

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

bail:
	return result;
}

waveform_t load_waveform_from_disk(arena_t *arena, string_t path)
{
	waveform_t result = {0};

    // uh oh
	// m_scoped(temp)
	{
		string_t file = fs_read_entire_file(temp, path);
		result = load_waveform_from_memory(arena, file);
	}

	return result;
}
