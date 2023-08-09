#ifndef PLUG_AUDIO_OUTPUT_H
#define PLUG_AUDIO_OUTPUT_H

#include "core/plug.h"

typedef void (*audio_output_callback_t)(size_t frame_count, float *frames);

typedef struct audio_output_i
{
	void (*start_audio_thread)(audio_output_callback_t callback);
} audio_output_i;

//
//
//

DREAM_DECLARE_PLUG(audio_output);

#endif
