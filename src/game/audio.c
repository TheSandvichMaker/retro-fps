#include "core/common.h"
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
					.id       = command->id,
					.waveform = command->sound.waveform,
					.volume   = command->sound.volume,
					.flags    = command->sound.flags,
					.p        = command->sound.p,
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
				listener_p = command->sound.p;
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

	zero_array(buffer, mix_channel_count*frames_to_mix);

	for (bd_iter_t it = bd_iter(&playing_sounds);
		 bd_iter_valid(&it);
		 )
	{
		playing_sound_t *playing  = it.data;
		waveform_t      *waveform = playing->waveform;

		uint32_t frames_left     = waveform->frame_count - playing->at_index;
		uint32_t frames_to_write = MIN(frames_to_mix, frames_left);

		float volume = playing->volume;

		int16_t *src = waveform->frames + waveform->channel_count*playing->at_index;
		float   *dst = buffer;

		bool sound_should_stop = false;

		for (size_t frame_index = 0; frame_index < frames_to_write; frame_index++)
		{
			//
			// process fades
			//

			float volume_mod = volume;

			for (fade_t **fade_at = &playing->first_fade;
			 	 *fade_at;
			 	 )
			{
				fade_t *fade = *fade_at;

				if (fade->frame_index < fade->frame_duration)
				{
					float t = (float)fade->frame_index / (float)fade->frame_duration;
					fade->frame_index++;

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

					fade_at = &(*fade_at)->next;
				}
				else
				{
					if (fade->flags & FADE_STOP_SOUND_WHEN_FINISHED)
					{
						sound_should_stop = true;
					}

					*fade_at = fade->next;
					bd_rem_item(&active_fades, fade);
				}
			}

			//
			// mix samples
			//

			for (size_t channel_index = 0; channel_index < waveform->channel_count; channel_index++)
			{
				int16_t sample = *src++;
				*dst++ = volume_mod*((float)sample / (float)INT16_MAX);
			}

			if (sound_should_stop)
			{
				break;
			}
		}

		playing->at_index += frames_to_write;
		bd_iter_next(&it);

		if (sound_should_stop || playing->at_index == waveform->frame_count)
		{
			stop_playing_sound_internal(playing);
		}
	}

	mixer_frame_index += frames_to_mix;
}
