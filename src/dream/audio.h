#ifndef DREAM_AUDIO_H
#define DREAM_AUDIO_H

#include "core/api_types.h"
#include "core/atomics.h"

typedef enum audio_channel_t
{
	A_CHANNEL_LEFT,
	A_CHANNEL_RIGHT,
	A_CHANNEL_COUNT,
} audio_channel_t;

typedef enum mixer_id_type_t
{
	MIXER_ID_TYPE_NONE,
	MIXER_ID_TYPE_PLAYING_SOUND,
	MIXER_ID_TYPE_COUNT,
} mixer_id_type_t;

#define MIXER_ID_INDEX_MASK (0x00FFFFFFFFFFFFFF)
#define MIXER_ID_TYPE_MASK  (0xFF00000000000000)
#define MIXER_ID_TYPE_SHIFT (56)

typedef struct mixer_id_t
{
	uint64_t value;
} mixer_id_t;

DREAM_INLINE uint64_t mixer_id_index(mixer_id_t id)
{
	return id.value & MIXER_ID_INDEX_MASK;
}

DREAM_INLINE uint64_t mixer_id_type(mixer_id_t id)
{
	return (id.value & MIXER_ID_TYPE_MASK) >> MIXER_ID_TYPE_SHIFT;
}

DREAM_INLINE mixer_id_t make_mixer_id(mixer_id_type_t type, uint64_t index)
{
	mixer_id_t result = {
		.value = ((uint64_t)type << MIXER_ID_TYPE_SHIFT)|(index & MIXER_ID_INDEX_MASK),
	};
	return result;
}

typedef enum play_sound_flags_t
{
	PLAY_SOUND_SPATIAL    = (1 << 0),
	PLAY_SOUND_FORCE_MONO = (1 << 1),
	PLAY_SOUND_LOOPING    = (1 << 2),
} play_sound_flags_t;

typedef enum fade_flags_t
{
	FADE_TARGET_VOLUME            = (1 << 0),

	//
	//
	//

	FADE_STOP_SOUND_WHEN_FINISHED = (1 << 16),
} fade_flags_t;

typedef enum fade_style_t
{
	FADE_STYLE_LINEAR,
	FADE_STYLE_SMOOTHSTEP,
	FADE_STYLE_SMOOTHERSTEP,
} fade_style_t;

typedef enum mix_command_kind_t
{
	MIX_PLAY_SOUND,
	MIX_STOP_SOUND,
	MIX_UPDATE_LISTENER,
	MIX_SOUND_POSITION,
	MIX_FADE,
	MIX_SET_PLAYING_SOUND_FLAGS,
} mix_command_kind_t;

typedef struct mix_command_play_sound_t
{
	waveform_t *waveform;

	float       volume;
	uint32_t    flags;

	v3_t        p;
	float       min_distance;
} mix_command_play_sound_t;

typedef struct mix_command_update_listener_t
{
	v3_t p;
	v3_t d;
} mix_command_update_listener_t;

typedef struct mix_command_sound_position_t
{
	v3_t p;
} mix_command_sound_position_t;

typedef struct mix_command_fade_t
{
	float        start;
	float        target;
	uint32_t     flags;
	fade_style_t style;
	uint32_t     duration;
} mix_command_fade_t;

typedef struct mix_command_set_playing_sound_flags_t
{
	uint32_t unset_flags; // unset_flags are applied first, so you can overwrite the flags by setting it to ~0
	uint32_t set_flags;
} mix_command_set_playing_sound_flags_t;

typedef struct mix_command_t
{
	mix_command_kind_t kind;
	mixer_id_t         id;

	union
	{
		mix_command_play_sound_t              play_sound;
		mix_command_update_listener_t         listener;
		mix_command_sound_position_t          sound_p;
		mix_command_fade_t                    fade;
		mix_command_set_playing_sound_flags_t set_playing_sound_flags;
	};
} mix_command_t;

#define MIXER_COMMAND_BUFFER_SIZE 4096

DREAM_API uint32_t g_mixer_next_playing_sound_id;
DREAM_API uint32_t g_mixer_command_read_index;
DREAM_API uint32_t g_mixer_command_write_index;
DREAM_API mix_command_t g_mixer_commands[MIXER_COMMAND_BUFFER_SIZE];

typedef struct play_sound_t
{
	waveform_t *waveform;
	float       volume;
	uint32_t    flags;
	v3_t        p;
	float       min_distance;
} play_sound_t;

DREAM_INLINE mix_command_t *push_mix_command(const mix_command_t *command)
{
	uint32_t command_index = g_mixer_command_write_index;
	copy_struct(&g_mixer_commands[command_index % MIXER_COMMAND_BUFFER_SIZE], command);

	COMPILER_BARRIER;

	atomic_increment_u32(&g_mixer_command_write_index);
	return &g_mixer_commands[command_index];
}

DREAM_INLINE mixer_id_t play_sound(const play_sound_t *params)
{
	if (g_mixer_next_playing_sound_id == 0)
		g_mixer_next_playing_sound_id = 1;

	mixer_id_t id = make_mixer_id(MIXER_ID_TYPE_PLAYING_SOUND, g_mixer_next_playing_sound_id++);

	mix_command_t command = {
		.kind = MIX_PLAY_SOUND,
		.id   = id,
		.play_sound = {
			.waveform     = params->waveform,
			.volume       = params->volume,
			.flags        = params->flags,
			.p            = params->p,
			.min_distance = params->min_distance,
		},
	};
	push_mix_command(&command);

	return id;
}

DREAM_INLINE uint32_t samples_from_seconds(float seconds)
{
	uint32_t result = (uint32_t)(seconds*WAVE_SAMPLE_RATE);
	return result;
}

DREAM_INLINE void stop_sound_harsh(mixer_id_t id)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	mix_command_t command = {
		.kind = MIX_STOP_SOUND,
		.id   = id,
	};
	push_mix_command(&command);
}

DREAM_INLINE void stop_sound(mixer_id_t id)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	push_mix_command(&(mix_command_t){
		.kind = MIX_FADE,
		.id   = id,
		.fade = {
			.flags    = FADE_TARGET_VOLUME|FADE_STOP_SOUND_WHEN_FINISHED,
			.style    = FADE_STYLE_LINEAR,
			.start    = 1.0f,
			.target   = 0.0f,
			.duration = 32,
		},
	});
}

DREAM_INLINE void fade_out_sound(mixer_id_t id, float fade_time)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	uint32_t duration = samples_from_seconds(fade_time);

	push_mix_command(&(mix_command_t){
		.kind = MIX_FADE,
		.id   = id,
		.fade = {
			.flags    = FADE_TARGET_VOLUME|FADE_STOP_SOUND_WHEN_FINISHED,
			.style    = FADE_STYLE_SMOOTHSTEP,
			.start    = 1.0f,
			.target   = 0.0f,
			.duration = duration,
		},
	});
}

DREAM_INLINE void set_listener(v3_t p, v3_t d)
{
	push_mix_command(&(mix_command_t){
		.kind = MIX_UPDATE_LISTENER,
		.listener = {
			.p = p,
			.d = d,
		},
	});
}

DREAM_INLINE void set_sound_position(mixer_id_t id, v3_t p)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	push_mix_command(&(mix_command_t){
		.kind = MIX_SOUND_POSITION,
		.id   = id,
		.sound_p = {
			.p = p,
		},
	});
}

DREAM_INLINE void update_playing_sound_flags(mixer_id_t id, uint32_t unset_flags, uint32_t set_flags)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	push_mix_command(&(mix_command_t){
		.kind = MIX_SET_PLAYING_SOUND_FLAGS,
		.id   = id,
		.set_playing_sound_flags = {
			.unset_flags = unset_flags,
			.set_flags   = set_flags,
		},
	});
}

DREAM_API void mix_samples(uint32_t frames_to_mix, float *buffer);

#endif
