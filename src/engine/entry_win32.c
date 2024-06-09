// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

//
// Unity build
//

#pragma warning(push, 0)

#include <stdio.h>
#include <stdbool.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <windowsx.h> // TODO: What god-forsaken shit am I including here
#include <dbghelp.h>
#include <shellapi.h>
#include <GameInput.h>

#pragma warning(pop)

#include "engine.h"

#include "core/core.c"
#include "game/game.c"

#include "cvar.c"
#include "audio_playback_win32.c"

//
//
//

#include "rhi/d3d12/rhi_d3d12.h"
#include "rhi/d3d12/rhi_d3d12.c"

//
//
//

global IGameInput *game_input;

global arena_t win32_arena;
global platform_hooks_t hooks;

typedef struct win32_input_context_t
{
	arena_t *arena;

	bool has_focus;

	platform_event_t *first_free_event;

	bool  cursor_locked;
	HWND  cursor_lock_window;
	POINT prev_cursor_point;
	bool  cursor_is_in_client_rect;

	input_t *input;
} win32_input_context_t;

fn_local void win32_lock_cursor(win32_input_context_t *context, HWND window, bool lock)
{
    if (context->cursor_locked != lock)
    {
        context->cursor_locked = lock;

        if (lock)
        {
			context->cursor_lock_window = window;

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
			context->cursor_lock_window = NULL;

            // ReleaseCapture();
            ShowCursor(TRUE);
            ClipCursor(NULL);
        }
    }
}

fn_local platform_event_t *new_event_internal(win32_input_context_t *context)
{
	if (!context->first_free_event)
	{
		context->first_free_event = m_alloc_struct_nozero(context->arena, platform_event_t);
		context->first_free_event->next = NULL;
	}

	platform_event_t *event = context->first_free_event;
	context->first_free_event = event->next;

	return event;
}

fn_local void push_event(win32_input_context_t *context, const platform_event_t *src_event)
{
	bool ctrl  = (bool)(GetAsyncKeyState(VK_CONTROL) & 0x8000);
	bool alt   = (bool)(GetAsyncKeyState(VK_MENU)    & 0x8000);
	bool shift = (bool)(GetAsyncKeyState(VK_SHIFT)   & 0x8000);

	platform_event_t *event = new_event_internal(context);
	copy_struct(event, src_event);

	event->ctrl  = ctrl;
	event->alt   = alt;
	event->shift = shift;

	sll_push_back(context->input->first_event, context->input->last_event, event);

	context->input->event_count += 1;
}

fn_local v2_t convert_mouse_cursor(POINT cursor, int height)
{
	v2_t result = {
		.x = (float)(cursor.x),
		.y = (float)(height - cursor.y - 1),
	};
	return result;
}

fn_local void new_input_frame(win32_input_context_t *context)
{
	input_t *input = context->input;

	input->event_count = 0;

	// free all events (TODO: don't need to do this this way anymore)
	while (input->first_event)
	{
		platform_event_t *event = input->first_event;
		input->first_event = event->next;

		event->next = context->first_free_event;
		context->first_free_event = event;
	}

	input->first_event = NULL;
	input->last_event  = NULL;

	input->mouse_dp    = (v2_t){ 0.0f, 0.0f };
	input->mouse_wheel = 0.0f;
}

fn_local void poll_input(win32_input_context_t *context, HWND window)
{
	input_t *input  = context->input;

#if 0
	GameInputGamepadButtons pressed_buttons  = 0;
	GameInputGamepadButtons released_buttons = 0;
	GameInputGamepadButtons down_buttons     = 0;

	if (game_input)
	{
		IGameInputDevice *device = NULL;

		IGameInputReading *reading;
		HRESULT hr = IGameInput_GetCurrentReading(game_input, GameInputKindGamepad, NULL, &reading);

		if (SUCCEEDED(hr))
		{
			GameInputGamepadState state;
			IGameInputReading_GetGamepadState(reading, &state);

			GameInputGamepadButtons changed_buttons       = prev_gamepad_state.buttons ^ state.buttons;
			GameInputGamepadButtons these_pressed_buttons = changed_buttons & state.buttons;

			down_buttons     |= state.buttons;
			pressed_buttons  |= these_pressed_buttons;
			released_buttons |= changed_buttons ^ these_pressed_buttons;

			prev_gamepad_state = state;

			if (!device)
			{
				IGameInputReading_GetDevice(reading, &device);
			}

			IGameInputReading *next_reading;
			hr = IGameInput_GetNextReading(game_input, reading, GameInputKindGamepad, device, &next_reading);

			IGameInputReading_Release(reading);

			reading = next_reading;
		}
	}

	if (pressed_buttons & GameInputGamepadA)
	{
		OutputDebugStringA("Da A iz da Prezxzed\n");
	}
#endif

	context->has_focus = (GetActiveWindow() == window);

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

	v2_t prev_mouse_p = convert_mouse_cursor(context->prev_cursor_point, height);
	v2_t mouse_p      = convert_mouse_cursor(cursor_point, height);
	v2_t mouse_dp     = sub(mouse_p, prev_mouse_p);

	if (context->has_focus && context->cursor_locked)
	{
		POINT mid_point;
		mid_point.x = (client_rect.left + client_rect.right) / 2;
		mid_point.y = (client_rect.bottom + client_rect.top) / 2;
		cursor_point = mid_point;

		ClientToScreen(window, &mid_point);
		SetCursorPos(mid_point.x, mid_point.y);
	}

	context->cursor_is_in_client_rect = cursor_is_in_client_rect;

	input->mouse_p  = mouse_p;
	input->mouse_dp = mouse_dp;

	context->prev_cursor_point = cursor_point;
}

typedef struct window_user_data_t
{
	rhi_window_t rhi_window;

	win32_input_context_t *input_context;
} window_user_data_t;

fn_local LRESULT window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	window_user_data_t *user = (window_user_data_t *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

	win32_input_context_t *context = user ? user->input_context : NULL;

	wchar_t last_char = 0;

	RECT client_rect;
	GetClientRect(hwnd, &client_rect);

	int height = client_rect.bottom - client_rect.top;

    switch (message)
    {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;

		case WM_MOUSEWHEEL:
		{
			if (!context) break;

			float delta = (float)GET_WHEEL_DELTA_WPARAM(wparam);

			context->input->mouse_wheel += delta;

			POINT mouse_point = {
				.x = GET_X_LPARAM(lparam),
				.y = GET_Y_LPARAM(lparam),
			};

			v2_t mouse_p = convert_mouse_cursor(mouse_point, height);

			push_event(context, &(platform_event_t){
				.kind                = Event_mouse_wheel,
				.mouse_wheel.wheel   = delta,
				.mouse_wheel.mouse_p = mouse_p,
			});
		} break;

		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		{
			if (!context) break;

			int  vk_code  = (int)wparam;
			bool pressed  = (bool)(message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
			bool repeated = (bool)(lparam & 0xFFFF);

			if (pressed && vk_code == VK_RETURN)
			{
				rhi_set_window_fullscreen(user->rhi_window, !rhi_get_window_fullscreen(user->rhi_window));
			}

			platform_event_t event = {
				.kind         = Event_key,
				.key.pressed  = pressed,
				.key.repeated = repeated,
				.key.keycode  = (keycode_t)vk_code,
			};

			push_event(context, &event);
		} break;

		case WM_MOUSEMOVE:
		{
			POINT mouse_point = {
				.x = GET_X_LPARAM(lparam),
				.y = GET_Y_LPARAM(lparam),
			};

			v2_t mouse_p = convert_mouse_cursor(mouse_point, height);

			platform_event_t event = {
				.kind = Event_mouse_move,
				.mouse_move.mouse_p = mouse_p,
			};

			push_event(context, &event);
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
			if (!context) break;

			mouse_button_t button = 0;

			switch (message)
			{
				case WM_LBUTTONDOWN: case WM_LBUTTONUP:
				{
					button = Button_left;
				} break;

				case WM_MBUTTONDOWN: case WM_MBUTTONUP:
				{
					button = Button_middle;
				} break;

				case WM_RBUTTONDOWN: case WM_RBUTTONUP:
				{
					button = Button_right;
				} break;

				case WM_XBUTTONDOWN: case WM_XBUTTONUP:
				{
					if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1)
					{
						button = Button_x1;
					}
					else
					{
						button = Button_x2;
					}
				} break;

				INVALID_DEFAULT_CASE;
			}

			bool pressed = !!(wparam & (MK_LBUTTON|MK_MBUTTON|MK_RBUTTON|MK_XBUTTON1|MK_XBUTTON2));

			POINT mouse_point = {
				.x = GET_X_LPARAM(lparam),
				.y = GET_Y_LPARAM(lparam),
			};

			v2_t mouse_p = convert_mouse_cursor(mouse_point, height);

			platform_event_t event = {
				.kind = Event_mouse_button,
				.mouse_button.mouse_p = mouse_p,
				.mouse_button.pressed = pressed,
				.mouse_button.button  = button,
			};

			push_event(context, &event);
		} break;

		case WM_CHAR:
		{
			if (!context) break;

			// laboured UTF-16 wrangling
			wchar_t chars[3] = {0};

			wchar_t curr_char = (wchar_t)wparam;
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

			platform_event_t event = {
				.kind = Event_text,
			};

			event.text.text.count = (size_t)WideCharToMultiByte(CP_UTF8, 0, chars, -1, event.text.text.data, 
																string_storage_size(event.text.text), NULL, NULL) - 1;

			push_event(context, &event);
		} break;

		case WM_SIZE:
		{
			uint32_t new_width  = LOWORD(lparam);
			uint32_t new_height = HIWORD(lparam);

			if (user)
			{
				rhi_resize_window(user->rhi_window, new_width, new_height);
			}
		} break;

        default:
        {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        } break;
    }

    return 0;
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

int wWinMain(HINSTANCE instance, 
             HINSTANCE prev_instance, 
             PWSTR     command_line, 
             int       command_show)
{
    IGNORED(instance);
    IGNORED(prev_instance);

	//GameInputCreate(&game_input);

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

	start_audio_thread(hooks.tick_audio);

    // create window

	HWND window = NULL;
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

	// it's d3d12 time

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
		rhi_window = rhi_init_window_d3d12(&(rhi_init_window_d3d12_params_t){
			.hwnd                                 = window,
			.create_frame_latency_waitable_object = true,
		});

		if (!RESOURCE_HANDLE_VALID(rhi_window))
		{
			return 1;
		}
	}
	else
	{
		return 1;
	}

    POINT prev_cursor_point;
    GetCursorPos(&prev_cursor_point);
    ScreenToClient(window, &prev_cursor_point);

	platform_cursor_t cursor      = Cursor_arrow;
	platform_cursor_t last_cursor = cursor;

	win32_input_context_t input_context = {
		.arena             = &win32_arena,
		.prev_cursor_point = prev_cursor_point,
		.input             = &(input_t){0},
	};

	window_user_data_t window_user_data = {
		.rhi_window    = rhi_window,
		.input_context = &input_context,
	};

	SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)&window_user_data);

    ShowWindow(window, command_show);

	input_t *input  = input_context.input;

	hires_time_t current_time = os_hires_time();

	uint64_t last_frame_sync_time = 0;

	rhi_frame_statistics_t frame_stats;
	if (rhi_get_frame_statistics(rhi_window, &frame_stats))
	{
		last_frame_sync_time = frame_stats.sync_cpu_time;
	}

	platform_init_io_t init_io = {0};

	hooks.init(&init_io);

	void *app_state = init_io.app_state;

    // message loop
    bool running = true;
    while (running)
    {
		rhi_wait_on_swap_chain(rhi_window);

		profiler_begin_frame();

		hires_time_t new_time = os_hires_time();

		double frame_time = os_seconds_elapsed(current_time, new_time);

		// subsitute frame time with exact vsync timing when possible

		rhi_frame_statistics_t frame_stats;
		if (rhi_get_frame_statistics(rhi_window, &frame_stats))
		{
			uint64_t freq = rhi_get_frame_sync_cpu_freq();
			uint64_t this_frame_sync_time = frame_stats.sync_cpu_time;
			frame_time = (double)(this_frame_sync_time - last_frame_sync_time) / (double)freq;

			last_frame_sync_time = this_frame_sync_time;
		}

		//

		// if (frame_time > 0.25)
		// {
		// 	frame_time = 0.25;
		// }

		current_time = new_time;

		// accumulator += frame_time;

		//
		// input
		//

		new_input_frame(&input_context);

		// pump messages

		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
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
		
		// poll gamepad / cursor

		poll_input(&input_context, window);

		//
		// tick
		//

		platform_tick_io_t tick_io = {
			.app_state     = app_state,
			.has_focus     = input_context.has_focus,
			.frame_time    = frame_time,
			.input         = input,
			.rhi_window    = rhi_window,
			.cursor_locked = input_context.cursor_locked,
		};

		hooks.tick(&tick_io);

		if (tick_io.request_exit)
		{
			running = false;
		}

		// process cursor updates

		if (tick_io.cursor_locked != input_context.cursor_locked)
		{
			win32_lock_cursor(&input_context, window, tick_io.cursor_locked);
		}

		if (input_context.cursor_is_in_client_rect)
		{
			if (cursor == Cursor_none)
			{
				SetCursor(NULL);
			}
			else
			{
				wchar_t *win32_cursor = IDC_ARROW;
				switch (cursor)
				{
					case Cursor_arrow:       win32_cursor = IDC_ARROW;    break;
					case Cursor_text_input:  win32_cursor = IDC_IBEAM;    break;
					case Cursor_resize_all:  win32_cursor = IDC_SIZEALL;  break;
					case Cursor_resize_ew:   win32_cursor = IDC_SIZEWE;   break;
					case Cursor_resize_ns:   win32_cursor = IDC_SIZENS;   break;
					case Cursor_resize_nesw: win32_cursor = IDC_SIZENESW; break;
					case Cursor_resize_nwse: win32_cursor = IDC_SIZENWSE; break;
					case Cursor_hand:        win32_cursor = IDC_HAND;     break;
					case Cursor_not_allowed: win32_cursor = IDC_NO;       break;
				}
				SetCursor(LoadCursor(NULL, win32_cursor));
			}
		}
		last_cursor = cursor;

		m_reset_temp_arenas();
    }

	rhi_flush_everything();

    return 0;
}

CREATE_PROFILER_TABLE
