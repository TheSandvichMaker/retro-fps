#pragma once

typedef struct editor_window_t editor_window_t;

typedef struct editor_ui_test_state_t
{
	float            slider_f32;
	int              slider_i32;
	char             edit_buffer_storage[256];
	dynamic_string_t edit_buffer;
} editor_ui_test_state_t;

fn void editor_do_ui_test_window(editor_ui_test_state_t *state, editor_window_t *window);

