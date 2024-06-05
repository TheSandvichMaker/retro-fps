#pragma once

typedef struct console_t
{
	font_atlas_t *font;
} console_t;

fn void update_and_draw_console(console_t *console, v2_t resolution, float dt);
