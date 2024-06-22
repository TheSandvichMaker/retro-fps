#pragma once

typedef struct console_t
{
	font_t *font;
} console_t;

typedef struct cvar_window_state_t
{
	string_storage_t(256) search_string_storage;
	dynamic_string_t      search_string;
} cvar_window_state_t;

fn void update_and_draw_console(console_t *console, v2_t resolution, float dt);
fn void editor_init_cvar_window(cvar_window_state_t *state);
fn void editor_do_cvar_window  (cvar_window_state_t *state, editor_window_t *window);
