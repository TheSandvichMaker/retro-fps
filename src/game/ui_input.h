// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef enum ui_event_kind_t
{
	UIEvent_none,
	UIEvent_mouse_move,
	UIEvent_mouse_button,
	UIEvent_key,
	UIEvent_text,
	UIEvent_COUNT,
} ui_event_kind_t;

typedef struct ui_event_t
{
	struct ui_event_t *next;

	ui_event_kind_t kind;

	bool pressed;
	bool ctrl;
	bool alt;
	bool shift;

	v2_t            mouse_p;
	mouse_buttons_t button;
	keycode_t       keycode;
	string_t        text;
} ui_event_t;

typedef struct ui_event_queue_t
{
	ui_event_t *head;
	ui_event_t *tail;
} ui_event_queue_t;

typedef struct ui_input_t
{
	ui_event_queue_t events;

	v2_t            mouse_p;
	v2_t            mouse_p_on_lmb;
	v2_t            mouse_p_on_mmb;
	v2_t            mouse_p_on_rmb;
	mouse_buttons_t buttons_pressed;
	mouse_buttons_t buttons_held;
	mouse_buttons_t buttons_released;
} ui_input_t;

fn void enqueue_ui_event(ui_event_queue_t *queue, ui_event_t *event);
fn void trickle_ui_input(ui_input_t *input, ui_event_queue_t *queue);
