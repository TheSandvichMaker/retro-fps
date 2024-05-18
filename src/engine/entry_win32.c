// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

//
// Unity build
//

#pragma warning(push, 0)

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dbghelp.h>
#include <shellapi.h>

#pragma warning(pop)

#include "engine.h"

#include "core/core.c"
#include "game/game.c"

#include "audio_playback_win32.c"
#include "d3d11.c"

//
//
//

#if DF_USE_RHI_ABSTRACTION
#include "rhi/d3d12/rhi_d3d12.h"
#include "rhi/d3d12/rhi_d3d12.c"
#endif

//
//
//

global arena_t win32_arena;
global platform_hooks_t hooks;

global bool has_focus;
global bool cursor_locked;
global HWND window;

fn_local void lock_cursor(bool lock)
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

typedef struct window_user_data_t
{
	rhi_window_t rhi_window;
} window_user_data_t;

fn_local LRESULT window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	window_user_data_t *user = (window_user_data_t *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

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

		case WM_SIZE:
		{
			uint32_t new_width  = LOWORD(lparam);
			uint32_t new_height = HIWORD(lparam);

#if DF_USE_RHI_ABSTRACTION
			if (user)
			{
				rhi_resize_window(user->rhi_window, new_width, new_height);
			}
#endif
		} break;

        default:
        {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        } break;
    }

    return 0;
}

fn_local v2_t convert_mouse_cursor(POINT cursor, int height)
{
	v2_t result = {
		.x = (float)(cursor.x),
		.y = (float)(height - cursor.y - 1),
	};
	return result;
}

fn_local BOOL CALLBACK enumerate_symbols_proc(SYMBOL_INFO *symbol_info, ULONG symbol_size, void *userdata)
{
	(void)userdata;

	debug_print("%08X %4u %s\n", symbol_info->Address, symbol_size, symbol_info->Name);
	return true;
}

fn_local void enumerate_symbols(void)
{
	HANDLE process = GetCurrentProcess();

	if (!SymInitialize(process, NULL, true))
		return;

	SymEnumSymbols(process, 0, "*!*", enumerate_symbols_proc, NULL);

	SymCleanup(process);
}

fn_local platform_event_t *new_event(arena_t *arena)
{
	platform_event_t *event = m_alloc_struct(arena, platform_event_t);
	event->ctrl  = (bool)(GetAsyncKeyState(VK_CONTROL) & 0x8000);
	event->alt   = (bool)(GetAsyncKeyState(VK_MENU)    & 0x8000);
	event->shift = (bool)(GetAsyncKeyState(VK_SHIFT)   & 0x8000);
	return event;
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

	platform_init((size_t)argc, argv, &hooks);

	if (!hooks.tick && !hooks.tick_audio)
	{
		FATAL_ERROR("No tick functions provided - can't do anything.");
	}

#if 0
    g_win32.wasapi_thread = CreateThread(NULL, 0, wasapi_thread_proc, NULL, 0, NULL);
#endif

	start_audio_thread(hooks.tick_audio);

    equip_render_api(d3d11_get_api());

    // create window
    {
        int desktop_w = GetSystemMetrics(SM_CXFULLSCREEN);
		(void)desktop_w;
        int desktop_h = GetSystemMetrics(SM_CYFULLSCREEN);
		(void)desktop_h;

        int w = 4*desktop_w / 5;
        int h = 4*desktop_h / 5;

        WNDCLASSEXW wclass = 
        {
            .cbSize        = sizeof(wclass),
            .style         = CS_HREDRAW|CS_VREDRAW,
            .lpfnWndProc   = window_proc,
            .hIcon         = LoadIconW(NULL, L"APPICON"),
            .hCursor       = NULL, 
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

	// it's d3d12 time

#if DF_USE_RHI_ABSTRACTION
	bool d3d12_success = rhi_init_d3d12(&(rhi_init_params_d3d12_t){
		.base = {
			.frame_latency = 2,
		},
		.enable_debug_layer          = true,
		.enable_gpu_based_validation = true,
	});

	rhi_window_t rhi_window = { 0 };

	if (d3d12_success)
	{
		rhi_window = rhi_init_window_d3d12(window);

		if (!RESOURCE_HANDLE_VALID(rhi_window))
		{
			return 1;
		}
	}
	else
	{
		return 1;
	}
#else
    // initialize d3d11
    {
        int result = init_d3d11(window);
        if (result < 0)
        {
            FATAL_ERROR("D3D11 initialization failed.");
        }
    }
#endif

	window_user_data_t window_user_data = {
		.rhi_window = rhi_window,
	};

	SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)&window_user_data);

    r_command_buffer_t r_commands = {
        .views_capacity        = R_VIEWS_CAPACITY,
        .views                 = m_alloc_array_nozero(&win32_arena, R_VIEWS_CAPACITY, r_view_t),

        .commands_capacity     = R_COMMANDS_CAPACITY,
        .commands              = m_alloc_array_nozero(&win32_arena, R_COMMANDS_CAPACITY, r_command_t),

        .data_capacity         = R_COMMANDS_DATA_CAPACITY,
        .data                  = m_alloc_nozero(&win32_arena, R_COMMANDS_DATA_CAPACITY, 16),

        .imm_indices_capacity  = R_IMMEDIATE_INDICES_CAPACITY,
        .imm_indices           = m_alloc_array_nozero(&win32_arena, R_IMMEDIATE_INDICES_CAPACITY, uint32_t),

        .imm_vertices_capacity = R_IMMEDIATE_VERTICES_CAPACITY,
        .imm_vertices          = m_alloc_array_nozero(&win32_arena, R_IMMEDIATE_VERTICES_CAPACITY, r_vertex_immediate_t),

        .ui_rects_capacity     = R_UI_RECTS_CAPACITY,
        .ui_rects              = m_alloc_array_nozero(&win32_arena, R_UI_RECTS_CAPACITY, r_ui_rect_t),
    };

    POINT prev_cursor_point;
    GetCursorPos(&prev_cursor_point);
    ScreenToClient(window, &prev_cursor_point);

	platform_cursor_t cursor      = PLATFORM_CURSOR_ARROW;
	platform_cursor_t last_cursor = cursor;

	hires_time_t start_time = os_hires_time();
	float dt = 1.0f / 60.0f;

    // message loop
    bool running = true;
    while (running)
    {
		arena_t *event_arena = m_get_temp(NULL, 0); // TODO: Think about how to provide events nicer (will be a ring buffer later probably for threaded fun)
		platform_event_t *first_event = NULL;
		platform_event_t * last_event = NULL;

		wchar_t last_char   = 0;
		size_t  event_count = 0;

		float   mouse_wheel = 0.0f;

        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
                case WM_QUIT:
                {
                    running = false;
                } break;

				case WM_MOUSEWHEEL:
				{
					mouse_wheel += (float)GET_WHEEL_DELTA_WPARAM(msg.wParam);
				} break;

				case WM_KEYDOWN:
				case WM_KEYUP:
				case WM_SYSKEYDOWN:
				case WM_SYSKEYUP:
				{
					int  vk_code  = (int)msg.wParam;
					bool pressed  = (bool)(msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN);
					bool repeated = (bool)(msg.lParam & 0xFFFF);

					platform_event_t *event = new_event(event_arena);
					event_count += 1;

					event->kind         = PLATFORM_EVENT_KEY;
					event->key.pressed  = pressed;
					event->key.repeated = repeated;
					event->key.keycode  = (platform_keycode_t)vk_code;

					sll_push_back_ex(first_event, last_event, event, next_);

                    TranslateMessage(&msg);
				} break;

				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
				case WM_MBUTTONDOWN:
				case WM_MBUTTONUP:
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
				case WM_XBUTTONDOWN:
				case WM_XBUTTONUP:
				{
					platform_event_t *event = new_event(event_arena);
					event_count += 1;

					platform_mouse_buttons_t button = 0;
					switch (msg.message)
					{
						case WM_LBUTTONDOWN: case WM_LBUTTONUP:
						{
							button = PLATFORM_MOUSE_BUTTON_LEFT;
						} break;

						case WM_MBUTTONDOWN: case WM_MBUTTONUP:
						{
							button = PLATFORM_MOUSE_BUTTON_MIDDLE;
						} break;

						case WM_RBUTTONDOWN: case WM_RBUTTONUP:
						{
							button = PLATFORM_MOUSE_BUTTON_RIGHT;
						} break;

						case WM_XBUTTONDOWN: case WM_XBUTTONUP:
						{
							if (GET_XBUTTON_WPARAM(msg.wParam) == XBUTTON1)
							{
								button = PLATFORM_MOUSE_BUTTON_X1;
							}
							else
							{
								button = PLATFORM_MOUSE_BUTTON_X2;
							}
						} break;

						INVALID_DEFAULT_CASE;
					}

					event->kind = PLATFORM_EVENT_MOUSE_BUTTON;
					event->mouse_button.pressed = !!(msg.wParam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON|MK_XBUTTON1|MK_XBUTTON2));
					event->mouse_button.button  = button;

					sll_push_back_ex(first_event, last_event, event, next_);
				} break;

				case WM_CHAR:
				{
					platform_event_t *event = new_event(event_arena);
					event_count += 1;

					event->kind = PLATFORM_EVENT_TEXT;

					// laboured UTF-16 wrangling
					{
						wchar_t chars[3] = {0};

						wchar_t curr_char = (wchar_t)msg.wParam;
						if (IS_HIGH_SURROGATE(curr_char))
						{
							last_char = curr_char;
						}
						else if (IS_LOW_SURROGATE(curr_char))
						{
							if (IS_SURROGATE_PAIR(last_char, curr_char))
							{
								chars[0] = last_char;
								chars[1] = curr_char;
							}
							last_char = 0;
						}
						else
						{
							chars[0] = curr_char;
							if (chars[0] == L'\r')
							{
								chars[0] = L'\n';
							}
						}

						event->text.text.count = (size_t)WideCharToMultiByte(CP_UTF8, 0, chars, -1, event->text.text.data, string_storage_size(event->text.text), NULL, NULL) - 1;
					}

					sll_push_back_ex(first_event, last_event, event, next_);
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

		(void)width;

        POINT cursor_point;
        GetCursorPos(&cursor_point);
        ScreenToClient(window, &cursor_point);

		bool cursor_is_in_client_rect = (cursor_point.x >  client_rect.left  &&
										 cursor_point.x <= client_rect.right &&
										 cursor_point.y >  client_rect.top   &&
										 cursor_point.y <= client_rect.bottom);

		v2_t prev_mouse_p = convert_mouse_cursor(prev_cursor_point, height);
		v2_t mouse_p      = convert_mouse_cursor(cursor_point, height);
		v2_t mouse_dp     = sub(mouse_p, prev_mouse_p);

		if (has_focus && cursor_locked)
		{
			POINT mid_point;
			mid_point.x = (client_rect.left + client_rect.right) / 2;
			mid_point.y = (client_rect.bottom + client_rect.top) / 2;
			cursor_point = mid_point;

			ClientToScreen(window, &mid_point);
			SetCursorPos(mid_point.x, mid_point.y);
        }

        prev_cursor_point = cursor_point;

#if DF_USE_RHI_ABSTRACTION
		rhi_begin_frame();

		rhi_command_list_t *command_list = rhi_get_command_list();
#endif

		platform_io_t tick_io = {
			.has_focus   = has_focus,

			.dt          = dt,

#if DF_USE_RHI_ABSTRACTION
			.rhi_window       = rhi_window,
			.rhi_command_list = command_list,
#endif

            .r_commands  = &r_commands,

			.mouse_p     = mouse_p,
			.mouse_dp    = mouse_dp,
			.mouse_wheel = mouse_wheel,

			// .gamepads..?,

			.event_count = event_count,
			.first_event = first_event,
			.last_event  = last_event,
			
			.cursor      = cursor,
		};

		hooks.tick(&tick_io);

        if (tick_io.lock_cursor != cursor_locked)
        {
            lock_cursor(tick_io.lock_cursor);
        }

		cursor = tick_io.cursor;

		if (cursor_is_in_client_rect)
		{
			if (cursor == PLATFORM_CURSOR_NONE)
			{
				SetCursor(NULL);
			}
			else
			{
				wchar_t *win32_cursor = IDC_ARROW;
				switch (cursor)
				{
					case PLATFORM_CURSOR_ARROW:       win32_cursor = IDC_ARROW;    break;
					case PLATFORM_CURSOR_TEXT_INPUT:  win32_cursor = IDC_IBEAM;    break;
					case PLATFORM_CURSOR_RESIZE_ALL:  win32_cursor = IDC_SIZEALL;  break;
					case PLATFORM_CURSOR_RESIZE_EW:   win32_cursor = IDC_SIZEWE;   break;
					case PLATFORM_CURSOR_RESIZE_NS:   win32_cursor = IDC_SIZENS;   break;
					case PLATFORM_CURSOR_RESIZE_NESW: win32_cursor = IDC_SIZENESW; break;
					case PLATFORM_CURSOR_RESIZE_NWSE: win32_cursor = IDC_SIZENWSE; break;
					case PLATFORM_CURSOR_HAND:        win32_cursor = IDC_HAND;     break;
					case PLATFORM_CURSOR_NOT_ALLOWED: win32_cursor = IDC_NO;       break;
				}
				SetCursor(LoadCursor(NULL, win32_cursor));
			}
		}
		last_cursor = cursor;

#if DF_USE_RHI_ABSTRACTION
		rhi_end_frame();
#else
        d3d11_execute_command_buffer(&r_commands, width, height);
        d3d11_present();
#endif

        if (tick_io.request_exit)
        {
            running = false;
        }

		m_reset_temp_arenas();

		hires_time_t end_time = os_hires_time();
		dt = (float)os_seconds_elapsed(start_time, end_time);
		dt = min(dt, 1.0f / 15.0f);

		start_time = end_time;
    }

    return 0;
}

CREATE_PROFILER_TABLE
