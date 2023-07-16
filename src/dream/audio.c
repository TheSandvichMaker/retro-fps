#include "core/common.h"
#include "core/arena.h"
#include "core/bulk_data.h"
#include "core/hashtable.h"
#include "core/math.h"

#include "asset.h"
#include "audio.h"

//
//
//

uint32_t g_mixer_next_playing_sound_id;
uint32_t g_mixer_command_read_index;
uint32_t g_mixer_command_write_index;
mix_command_t g_mixer_commands[MIXER_COMMAND_BUFFER_SIZE];

//
//
//

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
	fade_t  *first_fade;
} playing_sound_t;

typedef struct fade_t
{
	playing_sound_t *playing;
	fade_t          *next;

	uint32_t     flags;
	float        start;
	float        target;
	float        current;
	fade_style_t style;
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
	hash_rem(&playing_sound_from_id, playing->id.id);
}

void mix_samples(uint32_t frames_to_mix, float *buffer)
{
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
                playing_sound_t playing = {
                    .id           = command->id,
                    .waveform     = command->sound.waveform,
                    .volume       = command->sound.volume,
                    .flags        = command->sound.flags,
                    .p            = command->sound.p,
                    .min_distance = command->sound.min_distance,
                };

                resource_handle_t handle = bd_add_item(&playing_sounds, &playing);
                hash_add(&playing_sound_from_id, playing.id.id, handle.value);
			} break;

			case MIX_STOP_SOUND:
			{
				resource_handle_t handle;
				if (hash_find(&playing_sound_from_id, command->id.id, &handle.value))
				{
					playing_sound_t *playing = bd_get(&playing_sounds, handle);
					stop_playing_sound_internal(playing);
				}
			} break;

			case MIX_FADE:
			{
				resource_handle_t handle;
				if (hash_find(&playing_sound_from_id, command->id.id, &handle.value))
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
				}
			} break;

			case MIX_LISTENER_POSITION:
			{
				listener_p = command->listener.p;
				listener_d = normalize_or_zero(command->listener.d);
				basis_vectors(listener_d, make_v3(0, 0, 1), &listener_x, &listener_y, &listener_z);
			} break;

			case MIX_SOUND_POSITION:
			{
				resource_handle_t handle;
				if (hash_find(&playing_sound_from_id, command->id.id, &handle.value))
				{
					playing_sound_t *playing = bd_get(&playing_sounds, handle);
					playing->p = command->sound.p;
				}
			} break;
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

		v2_t volume = make_v2(playing->volume, playing->volume);

		if (playing->flags & PLAY_SOUND_SPATIAL)
		{
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

			float flatten = 1.0f - max(0.0f, m - distance_sq) / m;

			float cos_theta  = flatten*(to_sound.x*listener_x.x + to_sound.y*listener_x.y);
			float cos_theta2 = sqrt_ss(0.5f*(cos_theta + 1.0f));
			float sin_theta2 = sqrt_ss(1.0f - cos_theta2*cos_theta2);

			volume.x *= attenuation*sin_theta2;
			volume.y *= attenuation*cos_theta2;
		}

        bool sound_should_stop = false;

        for (size_t channel_index = 0; channel_index < mix_channel_count; channel_index++)
        {
            int16_t *start = NULL;

            if (waveform->channel_count == 1)
            {
                start = waveform_channel(waveform, 0);
            }
            else
            {
                start = waveform_channel(waveform, channel_index);
            }

            if (NEVER(!start))
                continue;

            start += playing->at_index;

            int16_t *end = start + waveform->frame_count;
            int16_t *src = start;
            float   *dst = mix_buffer + channel_index*frames_to_mix;

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
                        }
                    }
                }

                //
                // mix samples
                //

                v2_t final_volume = mul(volume, volume_mod);
                final_volume = max(0.0f, final_volume);

#if 0
                if (playing->flags & PLAY_SOUND_FORCE_MONO)
                {
                    float final = 0.0f;
                    float mod = 1.0f / (float)waveform->channel_count;

                    for (size_t channel_index = 0; channel_index < waveform->channel_count; channel_index++)
                    {
                        int16_t sample = *src++;

                        final += mod*((float)sample / (float)INT16_MAX);
                    }

                    for (size_t channel_index = 0; channel_index < mix_channel_count; channel_index++)
                    {
                        *dst++ += limit(final_volume.e[channel_index]*final);
                    }
                }
                else
#endif

                int16_t sample = *src++;
                *dst++ += final_volume.e[channel_index]*unorm_from_i16(sample);
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
                *dst++ = limit(*src_l++);
                *dst++ = limit(*src_r++);
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
}
