// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#include "rhi/rhi_api.h"

typedef enum mouse_button_t
{
	Button_none,

	Button_left,
	Button_middle,
	Button_right,
	Button_x1,
	Button_x2,

	Button_COUNT,
} mouse_button_t;

typedef enum keycode_t
{
    Key_lbutton        = 0x1,
    Key_rbutton        = 0x2,
    Key_cancel         = 0x3,
    Key_mbutton        = 0x4,
    Key_xbutton1       = 0x5,
    Key_xbutton2       = 0x6,
    Key_backspace      = 0x8,
    Key_tab            = 0x9,
    Key_clear          = 0xC,
    Key_return         = 0xD,
    Key_shift          = 0x10,
    Key_control        = 0x11,
    Key_alt            = 0x12,
    Key_pause          = 0x13,
    Key_capslock       = 0x14,
    Key_kana           = 0x15,
    Key_hangul         = 0x15,
    Key_junja          = 0x17,
    Key_final          = 0x18,
    Key_hanja          = 0x19,
    Key_kanji          = 0x19,
    Key_escape         = 0x1B,
    Key_convert        = 0x1C,
    Key_nonconvert     = 0x1D,
    Key_accept         = 0x1E,
    Key_modechange     = 0x1F,
    Key_space          = 0x20,
    Key_pageup         = 0x21,
    Key_pagedown       = 0x22,
    Key_end            = 0x23,
    Key_home           = 0x24,
    Key_left           = 0x25,
    Key_up             = 0x26,
    Key_right          = 0x27,
    Key_down           = 0x28,
    Key_select         = 0x29,
    Key_print          = 0x2A,
    Key_execute        = 0x2B,
    Key_printscreen    = 0x2C,
    Key_insert         = 0x2D,
    Key_delete         = 0x2E,
    Key_help           = 0x2F,
    /* 0x30 - 0x39: ascii numerals */
	Key_num_0          = 0x30,
	Key_num_1          = 0x31,
	Key_num_2          = 0x32,
	Key_num_3          = 0x33,
	Key_num_4          = 0x34,
	Key_num_5          = 0x35,
	Key_num_6          = 0x36,
	Key_num_7          = 0x37,
	Key_num_8          = 0x38,
	Key_num_9          = 0x39,
    /* 0x3A - 0x40: undefined */
    /* 0x41 - 0x5A: ascii alphabet */
	Key_a              = 0x41,
	Key_b              = 0x42,
	Key_c              = 0x43,
	Key_d              = 0x44,
	Key_e              = 0x45,
	Key_f              = 0x46,
	Key_g              = 0x47,
	Key_h              = 0x48,
	Key_i              = 0x49,
	Key_j              = 0x4A,
	Key_k              = 0x4B,
	Key_l              = 0x4C,
	Key_m              = 0x4D,
	Key_n              = 0x4E,
	Key_o              = 0x4F,
	Key_p              = 0x50,
	Key_q              = 0x51,
	Key_r              = 0x52,
	Key_s              = 0x53,
	Key_t              = 0x54,
	Key_u              = 0x55,
	Key_v              = 0x56,
	Key_w              = 0x57,
	Key_x              = 0x58,
	Key_y              = 0x59,
	Key_z              = 0x5A,
    Key_lsys           = 0x5B,
    Key_rsys           = 0x5C,
    Key_apps           = 0x5D,
    Key_sleep          = 0x5f,
    Key_numpad0        = 0x60,
    Key_numpad1        = 0x61,
    Key_numpad2        = 0x62,
    Key_numpad3        = 0x63,
    Key_numpad4        = 0x64,
    Key_numpad5        = 0x65,
    Key_numpad6        = 0x66,
    Key_numpad7        = 0x67,
    Key_numpad8        = 0x68,
    Key_numpad9        = 0x69,
    Key_multiply       = 0x6A,
    Key_add            = 0x6B,
    Key_separator      = 0x6C,
    Key_subtract       = 0x6D,
    Key_decimal        = 0x6E,
    Key_divide         = 0x6f,
    Key_f1             = 0x70,
    Key_f2             = 0x71,
    Key_f3             = 0x72,
    Key_f4             = 0x73,
    Key_f5             = 0x74,
    Key_f6             = 0x75,
    Key_f7             = 0x76,
    Key_f8             = 0x77,
    Key_f9             = 0x78,
    Key_f10            = 0x79,
    Key_f11            = 0x7A,
    Key_f12            = 0x7B,
    Key_f13            = 0x7C,
    Key_f14            = 0x7D,
    Key_f15            = 0x7E,
    Key_f16            = 0x7F,
    Key_f17            = 0x80,
    Key_f18            = 0x81,
    Key_f19            = 0x82,
    Key_f20            = 0x83,
    Key_f21            = 0x84,
    Key_f22            = 0x85,
    Key_f23            = 0x86,
    Key_f24            = 0x87,
    Key_numlock        = 0x90,
    Key_scroll         = 0x91,
    Key_lshift         = 0xA0,
    Key_rshift         = 0xA1,
    Key_lcontrol       = 0xA2,
    Key_rcontrol       = 0xA3,
    Key_lalt           = 0xA4,
    Key_ralt           = 0xA5,
    /* 0xA6 - 0xAC: browser keys, not sure what's up with that */
    Key_volumemute     = 0xAD,
    Key_volumedown     = 0xAE,
    Key_volumeup       = 0xAF,
    Key_medianexttrack = 0xB0,
    Key_mediaprevtrack = 0xB1,
    /* 0xB5 - 0xB7: "launch" keys, not sure what's up with that */
    Key_oem1           = 0xBA, // misc characters, us standard: ';:'
    Key_plus           = 0xBB,
    Key_comma          = 0xBC,
    Key_minus          = 0xBD,
    Key_period         = 0xBE,
    Key_oem2           = 0xBF, // misc characters, us standard: '/?'
    Key_oem3           = 0xC0, // misc characters, us standard: '~'
    /* 0xC1 - 0xDA: reserved / unassigned */
    /* 0xDB - 0xF5: more miscellanious OEM codes I'm ommitting for now */
    /* 0xF6 - 0xF9: keys I've never heard of */
    Key_play           = 0xFA,
    Key_zoom           = 0xFB,
    Key_oemclear       = 0xFE,
	Key_COUNT,
} keycode_t;

typedef enum platform_cursor_t
{
	Cursor_none,
	Cursor_arrow,
	Cursor_text_input,
	Cursor_resize_all,
	Cursor_resize_ew,
	Cursor_resize_ns,
	Cursor_resize_nesw,
	Cursor_resize_nwse,
	Cursor_hand,
	Cursor_not_allowed,
	Cursor_COUNT,
} platform_cursor_t;

typedef enum platform_event_kind_t
{
	Event_mouse_move,
	Event_mouse_wheel,
	Event_mouse_button,
	Event_key,
	Event_text,
	Event_COUNT,
} platform_event_kind_t;

typedef struct platform_event_mouse_move_t
{
	v2_t mouse_p;
} platform_event_mouse_move_t;

typedef struct platform_event_mouse_wheel_t
{
	float wheel;
	v2_t mouse_p;
} platform_event_mouse_wheel_t;

typedef struct platform_event_mouse_button_t
{
	v2_t           mouse_p;
	bool           pressed;
	mouse_button_t button;
} platform_event_mouse_button_t;

typedef struct platform_event_key_t
{
	bool      pressed;
	bool      repeated;
	keycode_t keycode;
} platform_event_key_t;

typedef struct platform_event_text_t
{
	string_storage_t(4) text;
} platform_event_text_t;

typedef struct platform_event_t
{
	struct platform_event_t *next; 

	platform_event_kind_t kind;

	bool ctrl;
	bool alt;
	bool shift;

	union
	{
		platform_event_mouse_move_t   mouse_move;
		platform_event_mouse_wheel_t  mouse_wheel;
		platform_event_mouse_button_t mouse_button;
		platform_event_key_t          key;
		platform_event_text_t         text;
	};
} platform_event_t;

typedef struct input_t
{
	// TODO: probably just remove all this in favor of just having events
	v2_t  mouse_p;
	v2_t  mouse_dp;
	float mouse_wheel;

	size_t event_count;
	platform_event_t *first_event;
	platform_event_t * last_event;
} input_t;

typedef struct platform_init_io_t 
{
	void *app_state;
	void *os_window_handle;
} platform_init_io_t;

typedef struct platform_tick_io_t
{
	void *app_state;

	// in
	bool     has_focus;
	double   frame_time;
	input_t *input;

	// rhi_window_t rhi_window;
	void *os_window_handle;

	// in-out
	bool              cursor_locked;
	platform_cursor_t cursor;

	// out
	v2_t    set_mouse_p;
	rect2_t restrict_mouse_rect;
	bool request_exit;
} platform_tick_io_t;

typedef struct platform_audio_io_t
{
	uint32_t frames_to_mix;
	float   *buffer;
} platform_audio_io_t;

typedef struct platform_hooks_t
{
	void (*init)      (platform_init_io_t  *io);
	void (*tick)      (platform_tick_io_t  *io);
	void (*tick_audio)(platform_audio_io_t *io);
} platform_hooks_t;

fn_export void platform_init(size_t argc, string_t *argv, platform_hooks_t *hooks);
