#ifndef DREAM_PLATFORM_H
#define DREAM_PLATFORM_H

#include "core/api_types.h"

typedef uint32_t platform_mouse_buttons_t;
typedef enum platform_mouse_button_enum_t
{
	PLATFORM_MOUSE_BUTTON_LEFT   = (1 << 0),
	PLATFORM_MOUSE_BUTTON_MIDDLE = (1 << 1),
	PLATFORM_MOUSE_BUTTON_RIGHT  = (1 << 2),
	PLATFORM_MOUSE_BUTTON_X1     = (1 << 3),
	PLATFORM_MOUSE_BUTTON_X2     = (1 << 4),
	PLATFORM_MOUSE_BUTTON_ANY    = (PLATFORM_MOUSE_BUTTON_LEFT|PLATFORM_MOUSE_BUTTON_MIDDLE|PLATFORM_MOUSE_BUTTON_RIGHT|PLATFORM_MOUSE_BUTTON_X1|PLATFORM_MOUSE_BUTTON_X2),
} platform_mouse_button_enum_t;

typedef enum platform_keycode_t
{
    PLATFORM_KEYCODE_LBUTTON        = 0x1,
    PLATFORM_KEYCODE_RBUTTON        = 0x2,
    PLATFORM_KEYCODE_CANCEL         = 0x3,
    PLATFORM_KEYCODE_MBUTTON        = 0x4,
    PLATFORM_KEYCODE_XBUTTON1       = 0x5,
    PLATFORM_KEYCODE_XBUTTON2       = 0x6,
    PLATFORM_KEYCODE_BACK           = 0x8,
    PLATFORM_KEYCODE_TAB            = 0x9,
    PLATFORM_KEYCODE_CLEAR          = 0xC,
    PLATFORM_KEYCODE_RETURN         = 0xD,
    PLATFORM_KEYCODE_SHIFT          = 0x10,
    PLATFORM_KEYCODE_CONTROL        = 0x11,
    PLATFORM_KEYCODE_ALT            = 0x12,
    PLATFORM_KEYCODE_PAUSE          = 0x13,
    PLATFORM_KEYCODE_CAPSLOCK       = 0x14,
    PLATFORM_KEYCODE_KANA           = 0x15,
    PLATFORM_KEYCODE_HANGUL         = 0x15,
    PLATFORM_KEYCODE_JUNJA          = 0x17,
    PLATFORM_KEYCODE_FINAL          = 0x18,
    PLATFORM_KEYCODE_HANJA          = 0x19,
    PLATFORM_KEYCODE_KANJI          = 0x19,
    PLATFORM_KEYCODE_ESCAPE         = 0x1B,
    PLATFORM_KEYCODE_CONVERT        = 0x1C,
    PLATFORM_KEYCODE_NONCONVERT     = 0x1D,
    PLATFORM_KEYCODE_ACCEPT         = 0x1E,
    PLATFORM_KEYCODE_MODECHANGE     = 0x1F,
    PLATFORM_KEYCODE_SPACE          = 0x20,
    PLATFORM_KEYCODE_PAGEUP         = 0x21,
    PLATFORM_KEYCODE_PAGEDOWN       = 0x22,
    PLATFORM_KEYCODE_END            = 0x23,
    PLATFORM_KEYCODE_HOME           = 0x24,
    PLATFORM_KEYCODE_LEFT           = 0x25,
    PLATFORM_KEYCODE_UP             = 0x26,
    PLATFORM_KEYCODE_RIGHT          = 0x27,
    PLATFORM_KEYCODE_DOWN           = 0x28,
    PLATFORM_KEYCODE_SELECT         = 0x29,
    PLATFORM_KEYCODE_PRINT          = 0x2A,
    PLATFORM_KEYCODE_EXECUTE        = 0x2B,
    PLATFORM_KEYCODE_PRINTSCREEN    = 0x2C,
    PLATFORM_KEYCODE_INSERT         = 0x2D,
    PLATFORM_KEYCODE_DELETE         = 0x2E,
    PLATFORM_KEYCODE_HELP           = 0x2F,
    /* 0x30 - 0x39: ascii numerals */
    /* 0x3A - 0x40: undefined */
    /* 0x41 - 0x5A: ascii alphabet */
    PLATFORM_KEYCODE_LSYS           = 0x5B,
    PLATFORM_KEYCODE_RSYS           = 0x5C,
    PLATFORM_KEYCODE_APPS           = 0x5D,
    PLATFORM_KEYCODE_SLEEP          = 0x5f,
    PLATFORM_KEYCODE_NUMPAD0        = 0x60,
    PLATFORM_KEYCODE_NUMPAD1        = 0x61,
    PLATFORM_KEYCODE_NUMPAD2        = 0x62,
    PLATFORM_KEYCODE_NUMPAD3        = 0x63,
    PLATFORM_KEYCODE_NUMPAD4        = 0x64,
    PLATFORM_KEYCODE_NUMPAD5        = 0x65,
    PLATFORM_KEYCODE_NUMPAD6        = 0x66,
    PLATFORM_KEYCODE_NUMPAD7        = 0x67,
    PLATFORM_KEYCODE_NUMPAD8        = 0x68,
    PLATFORM_KEYCODE_NUMPAD9        = 0x69,
    PLATFORM_KEYCODE_MULTIPLY       = 0x6A,
    PLATFORM_KEYCODE_ADD            = 0x6B,
    PLATFORM_KEYCODE_SEPARATOR      = 0x6C,
    PLATFORM_KEYCODE_SUBTRACT       = 0x6D,
    PLATFORM_KEYCODE_DECIMAL        = 0x6E,
    PLATFORM_KEYCODE_DIVIDE         = 0x6f,
    PLATFORM_KEYCODE_F1             = 0x70,
    PLATFORM_KEYCODE_F2             = 0x71,
    PLATFORM_KEYCODE_F3             = 0x72,
    PLATFORM_KEYCODE_F4             = 0x73,
    PLATFORM_KEYCODE_F5             = 0x74,
    PLATFORM_KEYCODE_F6             = 0x75,
    PLATFORM_KEYCODE_F7             = 0x76,
    PLATFORM_KEYCODE_F8             = 0x77,
    PLATFORM_KEYCODE_F9             = 0x78,
    PLATFORM_KEYCODE_F10            = 0x79,
    PLATFORM_KEYCODE_F11            = 0x7A,
    PLATFORM_KEYCODE_F12            = 0x7B,
    PLATFORM_KEYCODE_F13            = 0x7C,
    PLATFORM_KEYCODE_F14            = 0x7D,
    PLATFORM_KEYCODE_F15            = 0x7E,
    PLATFORM_KEYCODE_F16            = 0x7F,
    PLATFORM_KEYCODE_F17            = 0x80,
    PLATFORM_KEYCODE_F18            = 0x81,
    PLATFORM_KEYCODE_F19            = 0x82,
    PLATFORM_KEYCODE_F20            = 0x83,
    PLATFORM_KEYCODE_F21            = 0x84,
    PLATFORM_KEYCODE_F22            = 0x85,
    PLATFORM_KEYCODE_F23            = 0x86,
    PLATFORM_KEYCODE_F24            = 0x87,
    PLATFORM_KEYCODE_NUMLOCK        = 0x90,
    PLATFORM_KEYCODE_SCROLL         = 0x91,
    PLATFORM_KEYCODE_LSHIFT         = 0xA0,
    PLATFORM_KEYCODE_RSHIFT         = 0xA1,
    PLATFORM_KEYCODE_LCONTROL       = 0xA2,
    PLATFORM_KEYCODE_RCONTROL       = 0xA3,
    PLATFORM_KEYCODE_LALT           = 0xA4,
    PLATFORM_KEYCODE_RALT           = 0xA5,
    /* 0xA6 - 0xAC: browser keys, not sure what's up with that */
    PLATFORM_KEYCODE_VOLUMEMUTE     = 0xAD,
    PLATFORM_KEYCODE_VOLUMEDOWN     = 0xAE,
    PLATFORM_KEYCODE_VOLUMEUP       = 0xAF,
    PLATFORM_KEYCODE_MEDIANEXTTRACK = 0xB0,
    PLATFORM_KEYCODE_MEDIAPREVTRACK = 0xB1,
    /* 0xB5 - 0xB7: "launch" keys, not sure what's up with that */
    PLATFORM_KEYCODE_OEM1           = 0xBA, // misc characters, us standard: ';:'
    PLATFORM_KEYCODE_PLUS           = 0xBB,
    PLATFORM_KEYCODE_COMMA          = 0xBC,
    PLATFORM_KEYCODE_MINUS          = 0xBD,
    PLATFORM_KEYCODE_PERIOD         = 0xBE,
    PLATFORM_KEYCODE_OEM2           = 0xBF, // misc characters, us standard: '/?'
    PLATFORM_KEYCODE_OEM3           = 0xC0, // misc characters, us standard: '~'
    /* 0xC1 - 0xDA: reserved / unassigned */
    /* 0xDB - 0xF5: more miscellanious OEM codes I'm ommitting for now */
    /* 0xF6 - 0xF9: keys I've never heard of */
    PLATFORM_KEYCODE_PLAY           = 0xFA,
    PLATFORM_KEYCODE_ZOOM           = 0xFB,
    PLATFORM_KEYCODE_OEMCLEAR       = 0xFE,
	PLATFORM_KEY_COUNT,
} platform_keycode_t;

typedef enum platform_cursor_t
{
	PLATFORM_CURSOR_NONE,
	PLATFORM_CURSOR_ARROW,
	PLATFORM_CURSOR_TEXT_INPUT,
	PLATFORM_CURSOR_RESIZE_ALL,
	PLATFORM_CURSOR_RESIZE_EW,
	PLATFORM_CURSOR_RESIZE_NS,
	PLATFORM_CURSOR_RESIZE_NESW,
	PLATFORM_CURSOR_RESIZE_NWSE,
	PLATFORM_CURSOR_HAND,
	PLATFORM_CURSOR_NOT_ALLOWED,
	PLATFORM_CURSOR_COUNT,
} platform_cursor_t;

typedef enum platform_event_kind_t
{
	PLATFORM_EVENT_MOUSE_BUTTON,
	PLATFORM_EVENT_KEY,
	PLATFORM_EVENT_TEXT,
	PLATFORM_EVENT_COUNT,
} platform_event_kind_t;

typedef struct platform_event_t
{
	struct platform_event_t *next;

	platform_event_kind_t kind;

	union
	{
		struct
		{
			bool                     pressed;
			platform_mouse_buttons_t button;
		} mouse_button;

		struct
		{
			bool               pressed;
			platform_keycode_t keycode;
		} key;

		struct
		{
			string_storage_t(4) text;
		} text;
	};
} platform_event_t;

typedef enum platform_gamepad_axis_t
{
	PLATFORM_GAMEPAD_AXIS_LX,
	PLATFORM_GAMEPAD_AXIS_COUNT,
} platform_gamepad_axis_t;

typedef uint32_t platform_gamepad_buttons_t;
typedef enum platform_gamepad_button_enum_t
{
	PLATFORM_GAMEPAD_BUTTON_X = 1 << 0,
	// etc
} platform_gamepad_button_enum_t;

typedef struct platform_gamepad_t
{
	bool connected;

	float axes[PLATFORM_GAMEPAD_AXIS_COUNT];

	platform_gamepad_buttons_t buttons_pressed;
	platform_gamepad_buttons_t buttons_released;
	platform_gamepad_buttons_t buttons_down;
} platform_gamepad_t;

typedef struct platform_io_t
{
	bool has_focus;

	float dt;

	v2_t  mouse_p;
	v2_t  mouse_dp;
	float mouse_wheel;

	platform_gamepad_t gamepads[4];

	size_t event_count;
	platform_event_t *first_event;
	platform_event_t * last_event;

	platform_cursor_t cursor;

	bool lock_cursor;
	bool request_exit;
} platform_io_t;

typedef struct platform_audio_io_t
{
	uint32_t frames_to_mix;
	float *buffer;
} platform_audio_io_t;

typedef struct platform_hooks_t
{
	void (*tick)      (platform_io_t       *io);
	void (*tick_audio)(platform_audio_io_t *io);
} platform_hooks_t;

DREAM_DLLEXPORT void platform_init(size_t argc, string_t *argv, platform_hooks_t *hooks);

#endif
