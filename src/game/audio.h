// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

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

typedef enum sound_category_t
{
	SOUND_CATEGORY_GENERIC, // TODO: Not sure whether a generic sound category should exist (categorize everything properly!)
	SOUND_CATEGORY_EFFECTS,
	SOUND_CATEGORY_MUSIC,
	SOUND_CATEGORY_UI,
	SOUND_CATEGORY_COUNT,
} sound_category_t;

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

fn_local bool mixer_id_valid(mixer_id_t id)
{
    return id.value != 0;
}

fn_local uint64_t mixer_id_index(mixer_id_t id)
{
	return id.value & MIXER_ID_INDEX_MASK;
}

fn_local uint64_t mixer_id_type(mixer_id_t id)
{
	return (id.value & MIXER_ID_TYPE_MASK) >> MIXER_ID_TYPE_SHIFT;
}

fn_local mixer_id_t make_mixer_id(mixer_id_type_t type, uint64_t index)
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
	MIX_SET_CATEGORY_VOLUME,
	MIX_COMMAND_KIND_COUNT,
} mix_command_kind_t;

typedef struct mix_command_play_sound_t
{
	waveform_t *waveform;
	sound_category_t category;

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
	uint32_t     flags;
	fade_style_t style;

	float        start;
	float        target;

	uint32_t     duration;
} mix_command_fade_t;

typedef struct mix_command_set_playing_sound_flags_t
{
	uint32_t unset_flags; // unset_flags are applied first, so you can overwrite the flags by setting it to ~0
	uint32_t set_flags;
} mix_command_set_playing_sound_flags_t;

typedef struct mix_command_set_category_volume_t
{
	sound_category_t category;
	float            volume;
} mix_command_set_category_volume_t;

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
		// mix_command_set_category_volume_t     set_category_volume;
	};
} mix_command_t;

#define MIXER_COMMAND_BUFFER_SIZE 4096

typedef struct channel_matrix_t
{
	float m[A_CHANNEL_COUNT][A_CHANNEL_COUNT]; // [src][dst]
} channel_matrix_t;

fn_local channel_matrix_t channel_matrix_identity(void)
{
	channel_matrix_t result = {0};

	for (size_t i = 0; i < A_CHANNEL_COUNT; i++)
	{
		result.m[i][i] = 1.0f;
	}

	return result;
}

fn_local channel_matrix_t channel_matrix_mono(float value)
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

fn_local channel_matrix_t lerp_channel_matrix(channel_matrix_t a, channel_matrix_t b, float t)
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

	sound_category_t category;

	uint32_t at_index;

	float    volume;
	uint32_t flags;

	v3_t     p;
	float    min_distance;

	uint32_t fade_count;
	fade_t  *first_fade;
	fade_t  *last_fade;
} playing_sound_t;

typedef struct fade_t
{
	playing_sound_t *playing;
	fade_t          *next;
	fade_t          *prev;

	uint32_t     flags;
	fade_style_t style;

	float        start;
	float        target;
	float        current;
	uint32_t     frame_index;   
	uint32_t     frame_duration;
} fade_t;

typedef struct delay_t
{
	float    feedback;
	uint32_t index;
	uint32_t delay_time;
	float    buffer[DREAM_MIX_SAMPLE_RATE];
} delay_t;

typedef struct reverb_t
{
	float    feedback;
	uint32_t index_l;
	uint32_t index_r;
	uint32_t delay_time_l;
	uint32_t delay_time_r;
	float    buffer_l[DREAM_MIX_SAMPLE_RATE];
	float    buffer_r[DREAM_MIX_SAMPLE_RATE];
	float    mix_matrix[2][2];
} reverb_t;

typedef struct mixer_t
{
	alignas(64) atomic uint32_t command_read_index;
	alignas(64) atomic uint32_t command_write_index;
	alignas(64)

	float             category_volumes[SOUND_CATEGORY_COUNT];
	uint32_t          next_playing_sound_id;
	mix_command_t     commands[MIXER_COMMAND_BUFFER_SIZE];

	// don't touch these directly (unless you know what you're doing etc etc):

	pool_t  playing_sounds; // @must-initialize
	table_t playing_sounds_index;

	pool_t active_fades; // @must-initialize

	uint64_t frame_index;
	v3_t listener_p;
	v3_t listener_d;
	v3_t listener_x;
	v3_t listener_y;
	v3_t listener_z;

	reverb_t reverb;
	float    reverb_amount;
	float    reverb_feedback;
	int      reverb_delay_time;
	float    reverb_stereo_spread;
	float    reverb_diffusion_angle;
	float    filter_test;

	float filter_store_l;
	float filter_store_r;

	bool initialized;
} mixer_t;

fn mixer_t mixer;

// TODO: Deduplicate with mix_command_play_sound_t
typedef struct play_sound_t
{
	waveform_t      *waveform;
	sound_category_t category;
	float            volume;
	uint32_t         flags;
	v3_t             p;
	float            min_distance;
} play_sound_t;

fn_local bool write_mix_command(const mix_command_t *command)
{
	uint32_t w = atomic_load_explicit(&mixer.command_write_index, memory_order_relaxed);
	uint32_t r = atomic_load_explicit(&mixer.command_read_index,  memory_order_acquire);

	if (w - r == MIXER_COMMAND_BUFFER_SIZE)
	{
		return false;
	}

	mixer.commands[w % MIXER_COMMAND_BUFFER_SIZE] = *command;
	atomic_store_explicit(&mixer.command_write_index, w + 1, memory_order_release);

	return true;
}

fn_local bool read_mix_command(mix_command_t *command)
{
	uint32_t r = atomic_load_explicit(&mixer.command_read_index,  memory_order_relaxed);
	uint32_t w = atomic_load_explicit(&mixer.command_write_index, memory_order_acquire);

	if (w - r == 0)
	{
		return false;
	}

	*command = mixer.commands[r % MIXER_COMMAND_BUFFER_SIZE];
	atomic_store_explicit(&mixer.command_read_index, r + 1, memory_order_release);

	return true;
}

fn_local mixer_id_t play_sound(const play_sound_t *params)
{
	mixer_id_t id = make_mixer_id(MIXER_ID_TYPE_PLAYING_SOUND, mixer.next_playing_sound_id++);

	mix_command_t command = {
		.kind = MIX_PLAY_SOUND,
		.id   = id,
		.play_sound = {
			.waveform     = params->waveform,
			.category     = params->category,
			.volume       = params->volume,
			.flags        = params->flags,
			.p            = params->p,
			.min_distance = params->min_distance,
		},
	};
	write_mix_command(&command);

	return id;
}

fn_local uint32_t samples_from_seconds(float seconds)
{
	uint32_t result = (uint32_t)(seconds*DREAM_MIX_SAMPLE_RATE);
	return result;
}

fn_local void stop_sound_harsh(mixer_id_t id)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	mix_command_t command = {
		.kind = MIX_STOP_SOUND,
		.id   = id,
	};
	write_mix_command(&command);
}

fn_local void stop_sound(mixer_id_t id)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	write_mix_command(&(mix_command_t){
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

fn_local void fade_out_sound(mixer_id_t id, float fade_time)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	uint32_t duration = samples_from_seconds(fade_time);

	write_mix_command(&(mix_command_t){
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

fn_local void mixer_set_listener(v3_t p, v3_t d)
{
	write_mix_command(&(mix_command_t){
		.kind = MIX_UPDATE_LISTENER,
		.listener = {
			.p = p,
			.d = d,
		},
	});
}

fn_local void set_sound_position(mixer_id_t id, v3_t p)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	write_mix_command(&(mix_command_t){
		.kind = MIX_SOUND_POSITION,
		.id   = id,
		.sound_p = {
			.p = p,
		},
	});
}

fn_local void update_playing_sound_flags(mixer_id_t id, uint32_t unset_flags, uint32_t set_flags)
{
    if (!mixer_id_valid(id)) return;
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	write_mix_command(&(mix_command_t){
		.kind = MIX_SET_PLAYING_SOUND_FLAGS,
		.id   = id,
		.set_playing_sound_flags = {
			.unset_flags = unset_flags,
			.set_flags   = set_flags,
		},
	});
}

fn void mix_samples(uint32_t frames_to_mix, float *buffer);

#endif
