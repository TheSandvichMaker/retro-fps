// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef enum action_t
{
	Action_none,
	Action_left,
	Action_right,
	Action_forward,
	Action_back,
	Action_jump,
	Action_run,
	Action_crouch,
	Action_fire1,
	Action_fire2,
	Action_next_weapon,
	Action_prev_weapon,
	Action_weapon1,
	Action_weapon2,
	Action_weapon3,
	Action_weapon4,
	Action_weapon5,
	Action_weapon6,
	Action_weapon7,
	Action_weapon8,
	Action_weapon9,
	Action_escape,
	Action_toggle_noclip,
	Action_COUNT,
} action_t;

typedef enum action_axis_t
{
	ActionAxis_move_x,
	ActionAxis_move_y,
	ActionAxis_look_x,
	ActionAxis_look_y,
	ActionAxis_COUNT,
} action_axis_t;

typedef struct action_state_t
{
	bool pressed;
	bool held;
	bool released;
} action_state_t;

typedef struct action_system_t
{
	input_source_t input_source;

	float axis_states[ActionAxis_COUNT];

	v2_t  mouse_p;
	v2_t  mouse_dp;

	uint64_t       key_to_actions[Key_COUNT];
	action_state_t action_states [Action_COUNT];
} action_system_t;

global thread_local action_system_t *g_actions = NULL;

fn void equip_action_system  (action_system_t *action_system);
fn void unequip_action_system(void);

fn void bind_key_action  (action_t action, keycode_t key);
fn void unbind_key_action(action_t action, keycode_t key);

fn void process_action_system_input(input_t *input);
fn bool action_pressed (action_t action);
fn bool action_held    (action_t action);
fn bool action_released(action_t action);

fn v2_t get_movement_axes(void);
fn v2_t get_look_axes    (void);

fn v2_t get_action_mouse_p (void);
fn v2_t get_action_mouse_dp(void);

fn_local uint64_t action_bit(action_t action)
{
	return (1ull << action);
}
