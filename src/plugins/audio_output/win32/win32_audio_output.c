#pragma warning(push, 0)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define COBJMACROS
#define INITGUID
#include <mmdeviceapi.h>
#include <audioclient.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "ksuser.lib")

#pragma warning(pop)

#include <string.h>

#include "../audio_output.h"
#include "core/common.h"

const CLSID CLSID_MMDeviceEnumerator = {
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}
};
const IID IID_IMMDeviceEnumerator = {
    //MIDL_INTERFACE("A95664D2-9614-4F35-A746-DE8DB63617E6")
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
};
const IID IID_IAudioClient = {
    //MIDL_INTERFACE("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2")
    0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}
};
const IID IID_IAudioRenderClient = {
    //MIDL_INTERFACE("F294ACFC-3146-4483-A7BF-ADDCA7C260E2")
    0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}
};

static DWORD WINAPI wasapi_thread_proc(void *userdata)
{
    // TODO: What is COINIT_DISABLE_OLE1DDE doing for me?
    CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE);

	audio_output_callback_t callback = (audio_output_callback_t)userdata;

    HRESULT hr;

    // --------------------------------------------------------------------------------------------

    IMMDeviceEnumerator *device_enumerator;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, &device_enumerator);
    // ASSERT(SUCCEEDED(hr));

    // --------------------------------------------------------------------------------------------

    IMMDevice *audio_device;

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(device_enumerator, eRender, eConsole, &audio_device);
    // ASSERT(SUCCEEDED(hr));

    // --------------------------------------------------------------------------------------------

    IAudioClient *audio_client;

    hr = IMMDevice_Activate(audio_device, &IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, &audio_client);
    // ASSERT(SUCCEEDED(hr));

    // --------------------------------------------------------------------------------------------

    REFERENCE_TIME default_period;
    REFERENCE_TIME minimum_period;

    hr = IAudioClient_GetDevicePeriod(audio_client, &default_period, &minimum_period);
    // ASSERT(SUCCEEDED(hr));

#if 0
    WAVEFORMATEX *mix_format;

    hr = IAudioClient_GetMixFormat(audio_client, &mix_format);
    // ASSERT(SUCCEEDED(hr));

    WAVEFORMATEXTENSIBLE mix_format_ex;
    memcpy(&mix_format_ex, mix_format, sizeof(mix_format_ex));
#else
	WAVEFORMATEXTENSIBLE mix_format_ex = { 0 };

	mix_format_ex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	mix_format_ex.Format.nChannels = 2;          // Stereo
	mix_format_ex.Format.nSamplesPerSec = 44100; // Sample rate (Hz)
	mix_format_ex.Format.wBitsPerSample = 32;    // 32-bit float
	mix_format_ex.Format.nBlockAlign = (mix_format_ex.Format.nChannels * mix_format_ex.Format.wBitsPerSample) / 8;
	mix_format_ex.Format.nAvgBytesPerSec = mix_format_ex.Format.nSamplesPerSec * mix_format_ex.Format.nBlockAlign;
	mix_format_ex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE); // - sizeof(WAVEFORMATEX);

	mix_format_ex.Samples.wValidBitsPerSample = 32;
	mix_format_ex.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
	mix_format_ex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
#endif
    
#if 0
    CoTaskMemFree(mix_format);
#endif

    hr = IAudioClient_Initialize(audio_client, 
                                 AUDCLNT_SHAREMODE_SHARED, 
                                 (AUDCLNT_STREAMFLAGS_EVENTCALLBACK|
                                  AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|
                                  AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY), 
                                 minimum_period, 
                                 0, 
                                 (WAVEFORMATEX *)(&mix_format_ex), 
                                 NULL);
    // ASSERT(SUCCEEDED(hr));

    IAudioRenderClient *audio_render_client;

    hr = IAudioClient_GetService(audio_client, &IID_IAudioRenderClient, &audio_render_client);
    // ASSERT(SUCCEEDED(hr));

    UINT32 buffer_size;

    IAudioClient_GetBufferSize(audio_client, &buffer_size);

    HANDLE buffer_ready = CreateEventA(NULL, FALSE, FALSE, NULL);

    IAudioClient_SetEventHandle(audio_client, buffer_ready);

    // --------------------------------------------------------------------------------------------

    hr = IAudioClient_Start(audio_client);
    // ASSERT(SUCCEEDED(hr));

    while (WaitForSingleObject(buffer_ready, INFINITE) == WAIT_OBJECT_0)
    {
        UINT32 buffer_padding;

        hr = IAudioClient_GetCurrentPadding(audio_client, &buffer_padding);
        // ASSERT(SUCCEEDED(hr));

        UINT32 frame_count = buffer_size - buffer_padding;
        float *buffer;

        hr = IAudioRenderClient_GetBuffer(audio_render_client, frame_count, (BYTE **)&buffer);
        // ASSERT(SUCCEEDED(hr));

		callback(frame_count, buffer);

        hr = IAudioRenderClient_ReleaseBuffer(audio_render_client, frame_count, 0);
        // ASSERT(SUCCEEDED(hr));
    }

    return 0;
}

static void start_audio_thread(audio_output_callback_t callback)
{
	if (callback)
	{
		HANDLE thread = CreateThread(NULL, 0, wasapi_thread_proc, (void *)callback, 0, NULL);

		// TODO: keep track of the thread
		(void)thread;
	}
}

static audio_output_i audio_output = {
	.start_audio_thread = start_audio_thread,
};

DREAM_PLUG_LOAD(audio_output)
{
	(void)io;
	return &audio_output;
}

DREAM_PLUG_UNLOAD(audio_output)
{
	(void)io;
}