// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#include "input.h"

void equip_action_system(action_system_t *action_system)
{
	g_actions = action_system;
}

void unequip_action_system(void)
{
	equip_action_system(NULL);
}

void bind_key_action(action_t action, keycode_t key)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(key < Key_COUNT, "Passed invalid keycode to bind_key_action!");

	g_actions->key_to_actions[key] |= action_bit(action);
}

void unbind_key_action(action_t action, keycode_t key)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(key < Key_COUNT, "Passed invalid keycode to unbind_key_action!");

	g_actions->key_to_actions[key] &= ~action_bit(action);
}

void process_action_system_input(input_t *input)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	if (input->level > InputLevel_game)
	{
		log(ActionSystem, Warning, "Tried to process action system inputs using UI input level, that's probably a bug");
	}

	g_actions->mouse_p  = input->mouse_p;
	g_actions->mouse_dp = input->mouse_dp;

	for_array(key_index, input->keys)
	{
		button_t *key = &input->keys[key_index];

		uint64_t actions = g_actions->key_to_actions[key_index];

		for (uint64_t action_index = 0; action_index < 64; action_index++)
		{
			if (actions & (1ull << action_index))
			{
				action_state_t *action_state = &g_actions->action_states[action_index];
				action_state->pressed  = key->pressed;
				action_state->held     = key->held;
				action_state->released = key->released;
			}
		}
	}
}

bool action_pressed(action_t action)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_pressed");

	return g_actions->action_states[action].pressed;
}

bool action_held(action_t action)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_held");

	return g_actions->action_states[action].held;
}

bool action_released(action_t action)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_released");

	return g_actions->action_states[action].released;
}

v2_t get_movement_axes(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	v2_t result = {
		g_actions->axis_states[ActionAxis_move_x],
		g_actions->axis_states[ActionAxis_move_y],
	};

	return result;
}

v2_t get_look_axes(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	v2_t result = {
		g_actions->axis_states[ActionAxis_look_x],
		g_actions->axis_states[ActionAxis_look_y],
	};

	return result;
}

v2_t get_action_mouse_p(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	return g_actions->mouse_p;
}

v2_t get_action_mouse_dp(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	return g_actions->mouse_dp;
}
