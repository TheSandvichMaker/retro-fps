#pragma warning(push, 0)

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define COBJMACROS
#define INITGUID
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <shellapi.h>

#pragma warning(pop)

#include "core/core.h"

#include "d3d11.h"
#include "game/game.h"

static arena_t win32_arena;

static bool has_focus;
static bool cursor_locked;
static HWND window;

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

static void lock_cursor(bool lock)
{
    if (cursor_locked != lock)
    {
        cursor_locked = lock;
        if (lock)
        {
            // SetCapture(window);
            ShowCursor(FALSE);

            // good API, windows...

            RECT rect;
            GetClientRect(window, &rect);

            POINT screen_rect_lr;
            screen_rect_lr.x = rect.left;
            screen_rect_lr.y = rect.right;

            ClientToScreen(window, &screen_rect_lr);

            POINT screen_rect_tb;
            screen_rect_tb.x = rect.top;
            screen_rect_tb.y = rect.bottom;

            ClientToScreen(window, &screen_rect_tb);

            RECT screen_rect;
            screen_rect.left   = screen_rect_lr.x;
            screen_rect.right  = screen_rect_lr.y;
            screen_rect.top    = screen_rect_tb.x;
            screen_rect.bottom = screen_rect_tb.y;

            ClipCursor(&screen_rect);
        }
        else
        {
            // ReleaseCapture();
            ShowCursor(TRUE);
            ClipCursor(NULL);
        }
    }
}

LRESULT window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;

#if 0
        case WM_MOUSEACTIVATE:
        {
            return MA_ACTIVATEANDEAT;
        } break;
#endif

        default:
        {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        } break;
    }

    return 0;
}

typedef struct win32_state_t
{
    HANDLE wasapi_thread;
} win32_state_t;

static win32_state_t g_win32;

static DWORD WINAPI wasapi_thread_proc(void *userdata)
{
    // TODO: What is COINIT_DISABLE_OLE1DDE doing for me?
    CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE);

    (void)userdata;

    HRESULT hr;

    // --------------------------------------------------------------------------------------------

    IMMDeviceEnumerator *device_enumerator;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, &device_enumerator);
    ASSERT(SUCCEEDED(hr));

    // --------------------------------------------------------------------------------------------

    IMMDevice *audio_device;

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(device_enumerator, eRender, eConsole, &audio_device);
    ASSERT(SUCCEEDED(hr));

    // --------------------------------------------------------------------------------------------

    IAudioClient *audio_client;

    hr = IMMDevice_Activate(audio_device, &IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, &audio_client);
    ASSERT(SUCCEEDED(hr));

    // --------------------------------------------------------------------------------------------

    REFERENCE_TIME default_period;
    REFERENCE_TIME minimum_period;

    hr = IAudioClient_GetDevicePeriod(audio_client, &default_period, &minimum_period);
    ASSERT(SUCCEEDED(hr));

    WAVEFORMATEX *mix_format;

    hr = IAudioClient_GetMixFormat(audio_client, &mix_format);
    ASSERT(SUCCEEDED(hr));

    WAVEFORMATEXTENSIBLE mix_format_ex;

    copy_memory(&mix_format_ex, mix_format, sizeof(mix_format_ex));

    bool is_this_a_floating_point_format = !memcmp(&mix_format_ex.SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID));
    ASSERT(is_this_a_floating_point_format);
    ASSERT(mix_format_ex.Samples.wValidBitsPerSample == 32);

    CoTaskMemFree(mix_format);

    hr = IAudioClient_Initialize(audio_client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minimum_period, 0, (WAVEFORMATEX *)(&mix_format_ex), NULL);
    ASSERT(SUCCEEDED(hr));

    IAudioRenderClient *audio_render_client;

    hr = IAudioClient_GetService(audio_client, &IID_IAudioRenderClient, &audio_render_client);
    ASSERT(SUCCEEDED(hr));

    UINT32 buffer_size;

    IAudioClient_GetBufferSize(audio_client, &buffer_size);

    HANDLE buffer_ready = CreateEventA(NULL, FALSE, FALSE, NULL);

    IAudioClient_SetEventHandle(audio_client, buffer_ready);

    // --------------------------------------------------------------------------------------------

    hr = IAudioClient_Start(audio_client);
    ASSERT(SUCCEEDED(hr));

    while (WaitForSingleObject(buffer_ready, INFINITE) == WAIT_OBJECT_0)
    {
        UINT32 buffer_padding;

        hr = IAudioClient_GetCurrentPadding(audio_client, &buffer_padding);
        ASSERT(SUCCEEDED(hr));

        UINT32 frame_count = buffer_size - buffer_padding;
        float *buffer;

        hr = IAudioRenderClient_GetBuffer(audio_render_client, frame_count, (BYTE **)&buffer);
        ASSERT(SUCCEEDED(hr));

        game_audio_io_t game_audio_io = {
            .frames_to_mix = frame_count,
            .buffer        = buffer,
        };

        game_mix_audio(&game_audio_io);

        hr = IAudioRenderClient_ReleaseBuffer(audio_render_client, frame_count, 0);
        ASSERT(SUCCEEDED(hr));
    }

    return 0;
}

int wWinMain(HINSTANCE instance, 
             HINSTANCE prev_instance, 
             PWSTR     command_line, 
             int       command_show)
{
    IGNORED(instance);
    IGNORED(prev_instance);

    int argc;
    wchar_t **argv_wide = CommandLineToArgvW(command_line, &argc);

    string_t *argv = m_alloc_array(&win32_arena, argc, string_t);
    for (int i = 0; i < argc; i++)
    {
        argv[i] = utf8_from_utf16(&win32_arena, string16_from_cstr(argv_wide[i]));
    }

    string_t startup_map = strlit("test");

    for (int i = 0; i < argc; i++)
    {
        string_t arg = argv[i];
        startup_map = arg;
    }

    g_win32.wasapi_thread = CreateThread(NULL, 0, wasapi_thread_proc, NULL, 0, NULL);

    equip_render_api(d3d11_get_api());

    // create window
    {
        int w = 1920;
        int h = 1080;

        WNDCLASSEXW wclass = 
        {
            .cbSize        = sizeof(wclass),
            .style         = CS_HREDRAW|CS_VREDRAW,
            .lpfnWndProc   = window_proc,
            .hIcon         = LoadIconW(NULL, L"APPICON"),
            .hCursor       = LoadCursorW(NULL, IDC_ARROW),
            .lpszClassName = L"retro_window_class",
        };

        if (!RegisterClassExW(&wclass))
        {
            FATAL_ERROR("Failed to register window class");
        }

        RECT wrect = {
            .left   = 0,
            .top    = 0,
            .right  = w,
            .bottom = h,
        };

        AdjustWindowRect(&wrect, WS_OVERLAPPEDWINDOW, FALSE);

        window = CreateWindowExW(0, L"retro_window_class", L"retro fps",
                                 WS_OVERLAPPEDWINDOW,
                                 32, 32,
                                 wrect.right - wrect.left,
                                 wrect.bottom - wrect.top,
                                 NULL, NULL, NULL, NULL);

        if (!window)
        {
            FATAL_ERROR("Failed to create window");
        }
    }

    ShowWindow(window, command_show);

    // initialize d3d11
    {
        int result = init_d3d11(window);
        if (result < 0)
        {
            FATAL_ERROR("D3D11 initialization failed.");
        }
    }

    POINT prev_cursor;
    GetCursorPos(&prev_cursor);
    ScreenToClient(window, &prev_cursor);

    r_list_t r_list = { 0 };
    r_list.command_list_size = MB(16),
    r_list.command_list_base = m_alloc_nozero(&win32_arena, r_list.command_list_size, 16);
    r_list.max_immediate_icount = 1 << 24;
    r_list.immediate_indices = m_alloc_array_nozero(&win32_arena, r_list.max_immediate_icount, uint32_t);
    r_list.max_immediate_vcount = 1 << 23;
    r_list.immediate_vertices = m_alloc_array_nozero(&win32_arena, r_list.max_immediate_vcount, vertex_immediate_t);
    r_set_command_list(&r_list);

    // message loop
    bool running = true;
    while (running)
    {
        input_state_t input = { 0 };

        MSG msg;
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
                case WM_QUIT:
                {
                    running = false;
                } break;

                default:
                {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                } break;
            }
        }

        has_focus = (GetActiveWindow() == window);

        RECT client_rect;
        GetClientRect(window, &client_rect);

        int width  = client_rect.right - client_rect.left;
        int height = client_rect.bottom - client_rect.top;

        POINT cursor;
        GetCursorPos(&cursor);
        ScreenToClient(window, &cursor);

        if (has_focus)
        {
            if (GetAsyncKeyState('W'))        input.button_states |= BUTTON_FORWARD;
            if (GetAsyncKeyState('A'))        input.button_states |= BUTTON_LEFT;
            if (GetAsyncKeyState('S'))        input.button_states |= BUTTON_BACK;
            if (GetAsyncKeyState('D'))        input.button_states |= BUTTON_RIGHT;
            if (GetAsyncKeyState(VK_SPACE))   input.button_states |= BUTTON_JUMP;
            if (GetAsyncKeyState(VK_CONTROL)) input.button_states |= BUTTON_CROUCH;
            if (GetAsyncKeyState(VK_SHIFT))   input.button_states |= BUTTON_RUN;
            if (GetAsyncKeyState(VK_ESCAPE))  input.button_states |= BUTTON_ESCAPE;
            if (GetAsyncKeyState('V'))        input.button_states |= BUTTON_TOGGLE_NOCLIP;
            if (GetAsyncKeyState(VK_LBUTTON)) input.button_states |= BUTTON_FIRE1;
            if (GetAsyncKeyState(VK_RBUTTON)) input.button_states |= BUTTON_FIRE2;

            input.mouse_x = cursor.x;
            input.mouse_y = height - cursor.y - 1;

            input.mouse_dx =   cursor.x - prev_cursor.x;
            input.mouse_dy = -(cursor.y - prev_cursor.y);

            if (cursor_locked)
            {
                POINT mid_point;
                mid_point.x = (client_rect.left + client_rect.right) / 2;
                mid_point.y = (client_rect.bottom + client_rect.top) / 2;
                cursor = mid_point;

                ClientToScreen(window, &mid_point);
                SetCursorPos(mid_point.x, mid_point.y);
            }
        }

        for (UINT i = 0; i < 12; i++)
        {
            if (GetAsyncKeyState(VK_F1 + i))
            {
                input.button_states |= ((uint64_t)BUTTON_F1 << i);
            }
        }

        prev_cursor = cursor;

        r_reset_command_list();

        game_io_t io = {
            .startup_map = startup_map,

            .has_focus = has_focus,
            .input_state = &input,
        };

        game_tick(&io, 1.0f/60.0f);

        if (io.cursor_locked != cursor_locked)
        {
            lock_cursor(io.cursor_locked);
        }

        d3d11_draw_list(&r_list, width, height);
        d3d11_present();

        if (io.exit_requested)
        {
            running = false;
        }

        m_reset_and_decommit(temp);
    }

    return 0;
}
