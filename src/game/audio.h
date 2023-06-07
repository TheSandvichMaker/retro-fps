#include "core/api_types.h"
#include "core/atomics.h"

typedef struct mixer_id_t
{
	uint32_t id;
} mixer_id_t;

typedef enum play_sound_flags_t
{
	PLAY_SOUND_SPATIAL = 0x1,
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
	MIX_LISTENER_POSITION,
	MIX_SOUND_POSITION,
	MIX_FADE,
} mix_command_kind_t;

typedef struct mix_command_t
{
	mix_command_kind_t kind;
	mixer_id_t         id;

	union
	{
		struct
		{
			waveform_t *waveform;
			float       volume;
			uint32_t    flags;
			v3_t        p;
		} sound;

		struct
		{
			float        start;
			float        target;
			uint32_t     flags;
			fade_style_t style;
			uint32_t     duration;
		} fade;
	};
} mix_command_t;

#define MIXER_COMMAND_BUFFER_SIZE 4096

DREAM_API uint32_t g_mixer_next_playing_sound_id;
DREAM_API uint32_t g_mixer_command_read_index;
DREAM_API uint32_t g_mixer_command_write_index;
DREAM_API mix_command_t g_mixer_commands[MIXER_COMMAND_BUFFER_SIZE];

typedef struct play_sound_params_t
{
	waveform_t *waveform;
	float       volume;
	uint32_t    flags;
	v3_t        p;
} play_sound_params_t;

DREAM_INLINE mix_command_t *push_mix_command(const mix_command_t *command)
{
	uint32_t command_index = g_mixer_command_write_index;
	copy_struct(&g_mixer_commands[command_index], command);

	COMPILER_BARRIER;

	atomic_increment_u32(&g_mixer_command_write_index);
	return &g_mixer_commands[command_index];
}

DREAM_INLINE mixer_id_t play_sound(const play_sound_params_t *params)
{
	if (g_mixer_next_playing_sound_id == 0)
		g_mixer_next_playing_sound_id = 1;

	mixer_id_t id = { g_mixer_next_playing_sound_id++ };

	mix_command_t command = {
		.kind = MIX_PLAY_SOUND,
		.id   = id.id,
		.sound = {
			.waveform = params->waveform,
			.volume   = params->volume,
			.flags    = params->flags,
			.p        = params->p,
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
	mix_command_t command = {
		.kind = MIX_STOP_SOUND,
		.id   = id.id,
	};
	push_mix_command(&command);
}

DREAM_INLINE void stop_sound(mixer_id_t id)
{
	push_mix_command(&(mix_command_t){
		.kind = MIX_FADE,
		.id   = id.id,
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
	uint32_t duration = samples_from_seconds(fade_time);

	push_mix_command(&(mix_command_t){
		.kind = MIX_FADE,
		.id   = id.id,
		.fade = {
			.flags    = FADE_TARGET_VOLUME|FADE_STOP_SOUND_WHEN_FINISHED,
			.style    = FADE_STYLE_SMOOTHSTEP,
			.start    = 1.0f,
			.target   = 0.0f,
			.duration = duration,
		},
	});
}


DREAM_INLINE void set_listener_position(v3_t p)
{
	mix_command_t command = {
		.kind = MIX_LISTENER_POSITION,
		.sound = {
			.p = p,
		},
	};
	push_mix_command(&command);
}

DREAM_INLINE void set_sound_position(mixer_id_t id, v3_t p)
{
	mix_command_t command = {
		.kind = MIX_SOUND_POSITION,
		.id   = id.id,
		.sound = {
			.p = p,
		},
	};
	push_mix_command(&command);
}

DREAM_API void mix_samples(uint32_t frames_to_mix, float *buffer);
