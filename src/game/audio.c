// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#define MIXER_FLUSH_DENORMALS 1

//------------------------------------------------------------------------
// Mix Commands

bool write_mix_command(const mix_command_t *command)
{
	uint32_t w = atomic_load_explicit(&mixer.command_write_index, memory_order_relaxed);
	uint32_t r = atomic_load_explicit(&mixer.command_read_index,  memory_order_acquire);

	if (w - r == MIXER_COMMAND_BUFFER_SIZE)
	{
		log(Mixer, Warning, "Mixer command buffer is full. Did not write command - this probably means it is lost forever.");
		return false;
	}

	mixer.commands[w % MIXER_COMMAND_BUFFER_SIZE] = *command;
	atomic_store_explicit(&mixer.command_write_index, w + 1, memory_order_release);

	return true;
}

bool read_mix_command(mix_command_t *command)
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

mixer_id_t play_sound(const play_sound_t *params)
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

uint32_t samples_from_seconds(float seconds)
{
	uint32_t result = (uint32_t)(seconds*DREAM_MIX_SAMPLE_RATE);
	return result;
}

void stop_sound_harsh(mixer_id_t id)
{
	if (NEVER(mixer_id_type(id) != MIXER_ID_TYPE_PLAYING_SOUND)) return;

	mix_command_t command = {
		.kind = MIX_STOP_SOUND,
		.id   = id,
	};
	write_mix_command(&command);
}

void stop_sound(mixer_id_t id)
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

void fade_out_sound(mixer_id_t id, float fade_time)
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

void mixer_set_listener(v3_t p, v3_t d)
{
	write_mix_command(&(mix_command_t){
		.kind = MIX_UPDATE_LISTENER,
		.listener = {
			.p = p,
			.d = d,
		},
	});
}

void set_sound_position(mixer_id_t id, v3_t p)
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

void update_playing_sound_flags(mixer_id_t id, uint32_t unset_flags, uint32_t set_flags)
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

//------------------------------------------------------------------------
// Mixer

mixer_t mixer = {
	.playing_sounds         = INIT_POOL(playing_sound_t),
	.active_fades           = INIT_POOL(fade_t),
	.reverb_amount          = 0.25f,
	.reverb_feedback        = 0.85f,
	.reverb_delay_time      = 1433,
	.reverb_stereo_spread   = 1.276f,
	.reverb_diffusion_angle = 25.0f,
};

//

fn_local float limit(float s)
{
	s = max(-1.0f, min(1.0f, s));
	return s;
}

fn_local channel_matrix_t spatialize_channel_matrix(playing_sound_t *playing, channel_matrix_t in_matrix)
{
	waveform_t *waveform = playing->waveform;

	v3_t to_sound = sub(playing->p, mixer.listener_p);

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

	float cos_theta  = to_sound.x*mixer.listener_x.x + to_sound.y*mixer.listener_x.y;
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

fn_local void delay_init(delay_t *effect, float feedback, uint32_t delay_time)
{
	effect->feedback   = feedback;
	effect->delay_time = delay_time;
	effect->index      = 0;
	zero_array(&effect->buffer[0], ARRAY_COUNT(effect->buffer));
}

fn_local float delay_read(delay_t *effect)
{
	return effect->buffer[effect->index];
}

fn_local float delay_process(delay_t *effect, float input)
{
	size_t index    = effect->index;
	//float  feedback = effect->feedback;

	float output = effect->buffer[index];
	effect->buffer[index] = input; // + output*feedback;

	index += 1;
	if (index >= effect->delay_time)
	{
		index = 0;
	}

	effect->index = (uint32_t)index;

	return output;
}

fn_local void reverb_set_diffusion_angle(reverb_t *effect, float diffusion_angle)
{
	float s, c;
	sincos_ss(diffusion_angle, &s, &c);

	effect->mix_matrix[0][0] =  c;
	effect->mix_matrix[0][1] = -s;
	effect->mix_matrix[1][0] =  s;
	effect->mix_matrix[1][1] =  c;
}

fn_local void reverb_init(reverb_t *effect, float feedback, float diffusion_angle)
{
	effect->feedback     = feedback;
	effect->index_l      = 0;
	effect->index_r      = 0;
	effect->delay_time_l = 1433;
	effect->delay_time_r = 1433 + 23;
	zero_array(&effect->buffer_l[0], ARRAY_COUNT(effect->buffer_l));
	zero_array(&effect->buffer_r[0], ARRAY_COUNT(effect->buffer_r));
	reverb_set_diffusion_angle(effect, diffusion_angle);
}

fn_local v2_t reverb_process(reverb_t *effect, v2_t input)
{
	float l = effect->buffer_l[effect->index_l];
	float r = effect->buffer_r[effect->index_r];

	float l_mix = l*effect->mix_matrix[0][0] + r*effect->mix_matrix[0][1];
	float r_mix = l*effect->mix_matrix[1][0] + r*effect->mix_matrix[1][1];

	effect->buffer_l[effect->index_l] = input.x + l_mix*effect->feedback;
	effect->buffer_r[effect->index_r] = input.y + r_mix*effect->feedback;

	effect->index_l += 1; if (effect->index_l >= effect->delay_time_l) effect->index_l = 0;
	effect->index_r += 1; if (effect->index_r >= effect->delay_time_r) effect->index_r = 0;

	v2_t result = { l_mix, r_mix };
	return result;
}

fn_local void mixer_init(void)
{
	for (size_t i = 0; i < ARRAY_COUNT(mixer.category_volumes); i++)
	{
		mixer.category_volumes[i] = 1.0f;
	}

	mixer.initialized = true;

	reverb_init(&mixer.reverb, 0.9f, to_radians(15.0f));
}

fn_local void stop_playing_sound_internal(playing_sound_t *playing)
{
	while (playing->first_fade)
	{
		fade_t *fade = sll_pop(playing->first_fade);
		pool_rem_item(&mixer.active_fades, fade);
	}

	table_remove (&mixer.playing_sounds_index, playing->id.value);
	pool_rem_item(&mixer.playing_sounds, playing);
}

void mix_samples(uint32_t frames_to_mix, float *buffer)
{
#if MIXER_FLUSH_DENORMALS
	const uint32_t flush_to_zero_bit      = 1u << 15;
	const uint32_t denormals_are_zero_bit = 1u << 6;
	const uint32_t old_csr = _mm_getcsr();
	const uint32_t new_csr = old_csr|flush_to_zero_bit|denormals_are_zero_bit;
	_mm_setcsr(new_csr);
#endif

	if (!mixer.initialized)
	{
		mixer_init();
	}

	arena_t *temp = m_get_temp(NULL, 0);

	m_scope_begin(temp);

	size_t mix_channel_count = 2; // TODO: Surround sound!?

	//------------------------------------------------------------------------
	// Process commands
	//------------------------------------------------------------------------

	mix_command_t command;
	while (read_mix_command(&command))
	{
		switch (command.kind)
		{
			case MIX_PLAY_SOUND:
			{
				if (NEVER(mixer_id_type(command.id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				playing_sound_t *playing = pool_add(&mixer.playing_sounds);
				playing->id           = command.id;
				playing->waveform     = command.play_sound.waveform;
				playing->category     = command.play_sound.category;
				playing->volume       = command.play_sound.volume;
				playing->flags        = command.play_sound.flags;
				playing->p            = command.play_sound.p;
				playing->min_distance = command.play_sound.min_distance;

                table_insert_object(&mixer.playing_sounds_index, playing->id.value, playing);
			} break;

			case MIX_STOP_SOUND:
			{
				if (NEVER(mixer_id_type(command.id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				playing_sound_t *playing = table_find_object(&mixer.playing_sounds_index, command.id.value);

				if (ALWAYS(playing))
				{
					stop_playing_sound_internal(playing);
				}
			} break;

			case MIX_FADE:
			{
				if (NEVER(mixer_id_type(command.id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				playing_sound_t *playing = table_find_object(&mixer.playing_sounds_index, command.id.value);

				if (ALWAYS(playing))
				{
					fade_t *fade = pool_add(&mixer.active_fades);
					fade->playing        = playing;
					fade->flags          = command.fade.flags;
					fade->start          = command.fade.start;
					fade->target         = command.fade.target;
					fade->current        = command.fade.start;
					fade->style          = command.fade.style;
					fade->frame_duration = command.fade.duration;
					
					dll_push_back(playing->first_fade, playing->last_fade, fade);
					playing->fade_count += 1;
				}
			} break;

			case MIX_UPDATE_LISTENER:
			{
				mixer.listener_p = command.listener.p;
				mixer.listener_d = normalize_or_zero(command.listener.d);
				basis_vectors(mixer.listener_d, make_v3(0, 0, 1), &mixer.listener_x, &mixer.listener_y, &mixer.listener_z);
			} break;

			case MIX_SOUND_POSITION:
			{
				if (NEVER(mixer_id_type(command.id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				playing_sound_t *playing = table_find_object(&mixer.playing_sounds_index, command.id.value);

				if (ALWAYS(playing))
				{
					playing->p = command.sound_p.p;
				}
			} break;

			case MIX_SET_PLAYING_SOUND_FLAGS:
			{
				if (NEVER(mixer_id_type(command.id) != MIXER_ID_TYPE_PLAYING_SOUND)) break;

				playing_sound_t *playing = table_find_object(&mixer.playing_sounds_index, command.id.value);

				if (ALWAYS(playing))
				{
					playing->flags &= ~command.set_playing_sound_flags.unset_flags;
					playing->flags |=  command.set_playing_sound_flags.set_flags;
				}
			} break;

#if 0
			case MIX_SET_CATEGORY_VOLUME:
			{
				mix_command_set_category_volume_t *x = &command.set_category_volume;

				if (ALWAYS(x->category >= 0 && x->category < SOUND_CATEGORY_COUNT))
				{
					mix_category_volumes[x->category] = x->volume;
				}
			} break;
#endif

			INVALID_DEFAULT_CASE;
		}
	}

	//------------------------------------------------------------------------
	// Mix sounds
	//------------------------------------------------------------------------

    float *mix_buffer = m_alloc_array(temp, mix_channel_count*frames_to_mix, float);

	for (pool_iter_t it = pool_iter(&mixer.playing_sounds);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		playing_sound_t *playing  = it.data;
		waveform_t      *waveform = playing->waveform;

#if DREAM_SLOW
		{
			size_t actual_fade_count = 0;
			for (fade_t *fade = playing->first_fade; fade; fade = fade->next) actual_fade_count++;

			ASSERT(playing->fade_count == actual_fade_count);
		}
#endif

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

		float base_volume = playing->volume;
		base_volume *= mixer.category_volumes[playing->category];

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

				float sample = snorm_from_s16(*src++);

				float volume = base_volume*volume_mod;
				sample *= volume;

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

	for (pool_iter_t it = pool_iter(&mixer.active_fades);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
        fade_t *fade = it.data;

        playing_sound_t *playing  = fade->playing;
        waveform_t      *waveform = playing->waveform;

        if (waveform->frames)
        {
            if (fade->frame_index + frames_to_mix >= fade->frame_duration)
            {
				dll_remove(playing->first_fade, playing->last_fade, fade);
				playing->fade_count -= 1;

                pool_rem_item(&mixer.active_fades, fade);
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

	mixer.reverb.delay_time_l = (uint32_t)mixer.reverb_delay_time;
	mixer.reverb.delay_time_r = (uint32_t)((float)mixer.reverb_delay_time * mixer.reverb_stereo_spread);
	mixer.reverb.feedback = mixer.reverb_feedback;
	reverb_set_diffusion_angle(&mixer.reverb, to_radians(mixer.reverb_diffusion_angle));
	for (size_t i = 0; i < frames_to_mix; i++)
	{
		float l = buffer[2*i + 0];
		float r = buffer[2*i + 1];
		v2_t wet = reverb_process(&mixer.reverb, make_v2(mixer.reverb_amount*l, mixer.reverb_amount*r));
		buffer[2*i + 0] += wet.x;
		buffer[2*i + 1] += wet.y;
	}

	mixer.frame_index += frames_to_mix;

	m_scope_end(temp);

#if MIXER_FLUSH_DENORMALS
	_mm_setcsr(old_csr);
#endif
}
