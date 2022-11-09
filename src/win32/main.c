#include "core/core.h"

#pragma warning(push, 0)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <stdio.h>

#pragma warning(pop)

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

#include "d3d11.h"
#include "game/game.h"

static arena_t win32_arena;

static bool has_focus;
static bool cursor_locked;
static HWND window;

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

int wWinMain(HINSTANCE instance, 
             HINSTANCE prev_instance, 
             PWSTR     command_line, 
             int       command_show)
{
    IGNORED(instance);
    IGNORED(prev_instance);
    IGNORED(command_line);

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
    r_list.command_list_base = m_alloc(&win32_arena, r_list.command_list_size, 16);
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
            input.mouse_y = cursor.y;

            input.mouse_dx = cursor.x - prev_cursor.x;
            input.mouse_dy = cursor.y - prev_cursor.y;

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

        prev_cursor = cursor;

        r_reset_command_list();

        game_io_t io = {
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
