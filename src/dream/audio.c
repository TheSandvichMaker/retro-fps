#include "core/common.h"
#include "core/arena.h"
#include "core/bulk_data.h"
#include "core/hashtable.h"
#include "core/math.h"

#include "asset.h"
#include "audio.h"

#define MIXER_FLUSH_DENORMALS 1

//
//
//

alignas(64) float         g_mix_category_volumes[SOUND_CATEGORY_COUNT];
alignas(64) uint32_t      g_mixer_next_playing_sound_id;
alignas(64) uint32_t      g_mixer_command_read_index;
alignas(64) uint32_t      g_mixer_command_write_index;
alignas(64) mix_command_t g_mixer_commands[MIXER_COMMAND_BUFFER_SIZE];

//
//
//

typedef struct channel_matrix_t
{
	float m[A_CHANNEL_COUNT][A_CHANNEL_COUNT]; // [src][dst]
} channel_matrix_t;

DREAM_INLINE channel_matrix_t channel_matrix_identity(void)
{
	channel_matrix_t result = {0};

	for (size_t i = 0; i < A_CHANNEL_COUNT; i++)
	{
		result.m[i][i] = 1.0f;
	}

	return result;
}

DREAM_INLINE channel_matrix_t channel_matrix_mono(float value)
{
	channel_matrix_t result = {0};

	for (size_t i = 0; i < A_CHANNEL_COUNT; i++)
	{
		for (size_t j = 0; j < A_CHANNEL_COUNT; j++)
		{
			result.m[i][j] = value;
		}
	}

	return result;
}

DREAM_INLINE channel_matrix_t lerp_channel_matrix(channel_matrix_t a, channel_matrix_t b, float t)
{
	channel_matrix_t result;

	for (size_t i = 0; i < A_CHANNEL_COUNT; i++)
	{
		for (size_t j = 0; j < A_CHANNEL_COUNT; j++)
		{
			result.m[i][j] = lerp(a.m[i][j], b.m[i][j], t);
		}
	}

	return result;
}

typedef struct fade_t fade_t;

typedef struct playing_sound_t
{
	waveform_t *waveform;
	mixer_id_t id;

	uint32_t at_index;

	float    volume;
	uint32_t flags;

	v3_t     p;
	float    min_distance;

	uint32_t fade_count;
	fade_t  *first_fade;
} playing_sound_t;

typedef struct fade_t
{
	playing_sound_t *playing;
	fade_t          *next;

	uint32_t     flags;
	fade_style_t style;

	float        start;
	float        target;
	float        current;
	uint32_t     frame_index;   
	uint32_t     frame_duration;
} fade_t;

static bulk_t playing_sounds = INIT_BULK_DATA(playing_sound_t);
static hash_t playing_sound_from_id;
static bulk_t active_fades = INIT_BULK_DATA(fade_t);

static uint64_t mixer_frame_index;
static v3_t listener_p;
static v3_t listener_d;
static v3_t listener_x;
static v3_t listener_y;
static v3_t listener_z;

DREAM_INLINE float limit(float s)
{
	s = max(-1.0f, min(1.0f, s));
	return s;
}

static void stop_playing_sound_internal(playing_sound_t *playing)
{
	while (playing->first_fade)
	{
		fade_t *fade = playing->first_fade;
		playing->first_fade = fade->next;

		bd_rem_item(&active_fades, fade);
	}

	bd_rem_item(&playing_sounds, playing);
	hash_rem(&playing_sound_from_id, playing->id.value);
}

DREAM_INLINE channel_matrix_t spatialize_channel_matrix(playing_sound_t *playing, channel_matrix_t in_matrix)
{
	waveform_t *waveform = playing->waveform;

	v3_t to_sound = sub(playing->p, listener_p);

	float distance_sq = vlensq(to_sound);

	if (distance_sq > 0.01f)
	{
		to_sound = div(to_sound, sqrt_ss(distance_sq));
	}
	else
	{
		to_sound = (v3_t){ 0.0f, 0.0f, 0.0f };
	}

	float m = playing->min_distance;
	float attenuation = m / (m + distance_sq);

	float cos_theta  = to_sound.x*listener_x.x + to_sound.y*listener_x.y;
	float cos_theta2 = sqrt_ss(0.5f*(cos_theta + 1.0f));
	float sin_theta2 = sqrt_ss(1.0f - cos_theta2*cos_theta2);

	channel_matrix_t result = {0};

	for (size_t i = 0; i < waveform->channel_count; i++)
	{
		result.m[i][A_CHANNEL_LEFT]  = attenuation*sin_theta2;
		result.m[i][A_CHANNEL_RIGHT] = attenuation*cos_theta2;
	}

	float flatten = 0.2f*m / (0.2f*m + distance_sq);
	result = lerp_channel_matrix(result, in_matrix, flatten);

	return result;
}

static bool mixer_initialized;

static void mixer_init(void)
{
	for (size_t i = 0; i < ARRAY_COUNT(g_mix_category_volumes); i++)
	{
		g_mix_category_volumes[i] = 1.0f;
	}

	mixer_initialized = true;
}

void mix_samples(uint32_t frames_to_mix, float *buffer)
{
#if MIXER_FLUSH_DENORMALS
#define FLUSH_TO_ZERO_BIT      (1 << 15)
#define DENORMALS_ARE_ZERO_BIT (1 << 6)
	uint32_t old_csr = _mm_getcsr();
	uint32_t new_csr = old_csr|FLUSH_TO_ZERO_BIT|DENORMALS_ARE_ZERO_BIT;
	_mm_setcsr(new_csr);
#endif

	if (!mixer_initialized)
	{
		mixer_init();
	}

	size_t mix_channel_count = 2; // TODO: Surround sound!?

	//
	// Cycle Through Commands
	//

	uint32_t command_read_index  = g_mixer_command_read_index;
	uint32_t command_write_index = g_mixer_command_write_index;

	uint32_t command_index = command_read_index;
	while (command_index != command_write_index)
	{
		mix_command_t *command = &g_mixer_commands[command_index++ % MIXER_COMMAND_BUFFER_SIZE];

		switch (command->kind)
		{
			case MIX_PLAY_SOUND:
			{
				if (NEVER(mixer_id_type(command->id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

                playing_sound_t playing = {
                    .id           = command->id,
                    .waveform     = command->play_sound.waveform,
                    .volume       = command->play_sound.volume,
                    .flags        = command->play_sound.flags,
                    .p            = command->play_sound.p,
                    .min_distance = command->play_sound.min_distance,
                };

                resource_handle_t handle = bd_add_item(&playing_sounds, &playing);
                hash_add(&playing_sound_from_id, playing.id.value, handle.value);
			} break;

			case MIX_STOP_SOUND:
			{
				if (NEVER(mixer_id_type(command->id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				resource_handle_t handle;
				if (hash_find(&playing_sound_from_id, command->id.value, &handle.value))
				{
					playing_sound_t *playing = bd_get(&playing_sounds, handle);
					stop_playing_sound_internal(playing);
				}
			} break;

			case MIX_FADE:
			{
				if (NEVER(mixer_id_type(command->id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				resource_handle_t handle;
				if (hash_find(&playing_sound_from_id, command->id.value, &handle.value))
				{
					playing_sound_t *playing = bd_get(&playing_sounds, handle);

					resource_handle_t fade_handle = bd_add_item(&active_fades, &(fade_t){
						.playing        = playing,
						.flags          = command->fade.flags,
						.start          = command->fade.start,
						.target         = command->fade.target,
						.current        = command->fade.start,
						.style          = command->fade.style,
						.frame_duration = command->fade.duration,
					});
					fade_t *fade = bd_get(&active_fades, fade_handle);

					fade->next = playing->first_fade;
					playing->first_fade = fade;
					playing->fade_count += 1;
				}
			} break;

			case MIX_UPDATE_LISTENER:
			{
				listener_p = command->listener.p;
				listener_d = normalize_or_zero(command->listener.d);
				basis_vectors(listener_d, make_v3(0, 0, 1), &listener_x, &listener_y, &listener_z);
			} break;

			case MIX_SOUND_POSITION:
			{
				if (NEVER(mixer_id_type(command->id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				resource_handle_t handle;
				if (hash_find(&playing_sound_from_id, command->id.value, &handle.value))
				{
					playing_sound_t *playing = bd_get(&playing_sounds, handle);
					playing->p = command->sound_p.p;
				}
			} break;

			case MIX_SET_PLAYING_SOUND_FLAGS:
			{
				if (NEVER(mixer_id_type(command->id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				resource_handle_t handle;
				if (hash_find(&playing_sound_from_id, command->id.value, &handle.value))
				{
					playing_sound_t *playing = bd_get(&playing_sounds, handle);
					playing->flags &= ~command->set_playing_sound_flags.unset_flags;
					playing->flags |=  command->set_playing_sound_flags.set_flags;
				}
			} break;

#if 0
			case MIX_SET_CATEGORY_VOLUME:
			{
				mix_command_set_category_volume_t *x = &command->set_category_volume;

				if (ALWAYS(x->category >= 0 && x->category < SOUND_CATEGORY_COUNT))
				{
					mix_category_volumes[x->category] = x->volume;
				}
			} break;
#endif

			INVALID_DEFAULT_CASE;
		}
	}

	g_mixer_command_read_index = command_index;

	//
	// Mix Sounds
	//

    float *mix_buffer = m_alloc_array(temp, mix_channel_count*frames_to_mix, float);

	for (bd_iter_t it = bd_iter(&playing_sounds);
		 bd_iter_valid(&it);
		 bd_iter_next(&it))
	{
		playing_sound_t *playing  = it.data;
		waveform_t      *waveform = playing->waveform;

        if (!waveform->frames)
            continue;

		uint32_t frames_left = (uint32_t)waveform->frame_count - playing->at_index;

		uint32_t frames_to_write;
		if (playing->flags & PLAY_SOUND_LOOPING)
		{
			frames_to_write = frames_to_mix;
		}
		else
		{
			frames_to_write = MIN(frames_to_mix, frames_left);
		}

		channel_matrix_t channel_matrix = {0};

		if (playing->flags & PLAY_SOUND_FORCE_MONO)
		{
			channel_matrix = channel_matrix_mono(0.707f);
		}
		else
		{
			channel_matrix = channel_matrix_identity();
		}

		if (playing->flags & PLAY_SOUND_SPATIAL)
		{
			channel_matrix = spatialize_channel_matrix(playing, channel_matrix);
		}

        bool sound_should_stop = false;

		for (size_t src_channel_index = 0; src_channel_index < waveform->channel_count; src_channel_index++)
		{
			int16_t *start = waveform_channel(waveform, src_channel_index);

			if (NEVER(!start))
				continue;

			start += playing->at_index;

			int16_t *end = start + waveform->frame_count;
			int16_t *src = start;

			for (size_t frame_index = 0; frame_index < frames_to_write; frame_index++)
			{
				// wrap looping sounds

				if (src >= end)
				{
					size_t diff = end - src;
					src = start + diff;
				}

				//
				// process fades
				//

				float volume_mod = 1.0f;

				for (fade_t *fade = playing->first_fade; fade; fade = fade->next)
				{
					size_t fade_frame_index = fade->frame_index + frame_index;

					if (fade_frame_index < fade->frame_duration)
					{
						float t = (float)fade_frame_index / (float)fade->frame_duration;

						switch (fade->style)
						{
							case FADE_STYLE_LINEAR:
							{
								// nothing to do
							} break;

							case FADE_STYLE_SMOOTHSTEP:
							{
								t = smoothstep(t);
							} break;

							case FADE_STYLE_SMOOTHERSTEP:
							{
								t = smootherstep(t);
							} break;
						}

						float fade_value = lerp(fade->start, fade->target, t);
						
						if (fade->flags & FADE_TARGET_VOLUME)
						{
							volume_mod *= fade_value;
						}
					}
					else
					{
						if (fade->flags & FADE_STOP_SOUND_WHEN_FINISHED)
						{
							sound_should_stop = true;
							volume_mod        = 0.0f;
						}
					}
				}

				//
				// mix samples
				//

				float sample = unorm_from_i16(*src++);
				sample *= playing->volume;

				for (size_t dst_channel_index = 0; dst_channel_index < mix_channel_count; dst_channel_index++)
				{
					float mix_mapping = channel_matrix.m[src_channel_index][dst_channel_index];

					float *dst = mix_buffer + dst_channel_index*frames_to_mix + frame_index;
					*dst += mix_mapping*sample;
				}
			}
        }

		playing->at_index += frames_to_write;

		if (playing->flags & PLAY_SOUND_LOOPING)
		{
			playing->at_index %= waveform->frame_count;
		}
		else if (playing->at_index == waveform->frame_count)
		{
			sound_should_stop = true;
		}

		if (sound_should_stop)
		{
			stop_playing_sound_internal(playing);
		}
	}

	for (bd_iter_t it = bd_iter(&active_fades);
		 bd_iter_valid(&it);
		 bd_iter_next(&it))
	{
        fade_t *fade = it.data;

        playing_sound_t *playing  = fade->playing;
        waveform_t      *waveform = playing->waveform;

        if (waveform->frames)
        {
            if (fade->frame_index + frames_to_mix >= fade->frame_duration)
            {
                bd_rem_item(&active_fades, fade);

                bool removed_from_playing = false;

                // TODO: maybe don't linear search
                for (fade_t **remove_at = &fade->playing->first_fade; *remove_at; remove_at = &(*remove_at)->next)
                {
                    if (*remove_at == fade)
                    {
                        removed_from_playing = true;
                        sll_pop(*remove_at);
						if (ALWAYS(playing->fade_count > 0)) playing->fade_count -= 1;
                        break;
                    }
                }

                ASSERT(removed_from_playing); // sanity test
            }
            else
            {
                fade->frame_index += frames_to_mix;
            }
        }
    }

    // output

    {
        float *dst = buffer;
        if (mix_channel_count == 2)
        {
            float *src_l = mix_buffer;
            float *src_r = mix_buffer + frames_to_mix;
            for (size_t frame_index = 0; frame_index < frames_to_mix; frame_index++)
            {
				float l = *src_l++;
				float r = *src_r++;

                *dst++ = limit(l);
                *dst++ = limit(r);
            }
        }
        else
        {
            for (size_t frame_index = 0; frame_index < frames_to_mix; frame_index++)
            {
                for (size_t channel_index = 0; channel_index < mix_channel_count; channel_index++)
                {
                    float *src = mix_buffer + channel_index*frames_to_mix + frame_index;
                    *dst++ = limit(*src);
                }
            }
        }
    }

	mixer_frame_index += frames_to_mix;

#if MIXER_FLUSH_DENORMALS
	_mm_setcsr(old_csr);
#endif
}
