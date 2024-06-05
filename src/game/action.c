void equip_action_system(action_system_t *action_system)
{
	g_actions = action_system;
}

void unequip_action_system(void)
{
	equip_action_system(NULL);
}

action_system_t *get_action_system(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	return g_actions;
}

void bind_key_action(action_t action, keycode_t key)
{
	action_system_t *actions = get_action_system();

	ASSERT_MSG(key < Key_COUNT, "Passed invalid keycode to bind_key_action!");
	actions->key_to_actions[key] |= action_bit(action);
}

void unbind_key_action(action_t action, keycode_t key)
{
	action_system_t *actions = get_action_system();

	ASSERT_MSG(key < Key_COUNT, "Passed invalid keycode to unbind_key_action!");
	actions->key_to_actions[key] &= ~action_bit(action);
}

void bind_mouse_button_action(action_t action, mouse_button_t button)
{
	action_system_t *actions = get_action_system();

	ASSERT_MSG(button < Button_COUNT, "Passed invalid button to bind_mouse_button_action!");
	actions->button_to_actions[button] |= action_bit(action);
}

void unbind_mouse_button_action(action_t action, mouse_button_t button)
{
	action_system_t *actions = get_action_system();

	ASSERT_MSG(button < Button_COUNT, "Passed invalid button to unbind_mouse_button_action!");
	actions->button_to_actions[button] &= ~action_bit(action);
}

void ingest_action_system_input(input_t *input)
{
	action_system_t *actions = get_action_system();

	actions->mouse_p  = input->mouse_p;
	actions->mouse_dp = input->mouse_dp;

	// TODO: how to handle axes nicely
	actions->axis_states[ActionAxis_look_x] = actions->mouse_dp.x;
	actions->axis_states[ActionAxis_look_y] = actions->mouse_dp.y;

	for (platform_event_t *event = input->first_event;
		 event;
		 event = event->next)
	{
		if (event->kind == Event_key)
		{
			keycode_t keycode      = event->key.keycode;
			uint64_t  actions_mask = actions->key_to_actions[keycode];

			if (actions_mask)
			{
				// TODO: bit scan forward or something, maybe. Daniel Lemire has a blog post about quickly scanning bit vectors.
				// here:      https://lemire.me/blog/2018/02/21/iterating-over-set-bits-quickly/
				// also this: https://lemire.me/blog/2018/03/08/iterating-over-set-bits-quickly-simd-edition/
				for (uint64_t action_index = 0; action_index < 64; action_index++)
				{
					if (actions_mask & (1ull << action_index))
					{
						action_state_t *action_state = &actions->action_states[action_index];
						if (event->key.pressed)
						{
							action_state->pressed  |= true;
						}
						else
						{
							action_state->released |= true;
						}
						action_state->held = event->key.pressed;
					}
				}
			}
		}
		else if (event->kind == Event_mouse_button)
		{
			mouse_button_t button       = event->mouse_button.button;
			uint64_t       actions_mask = actions->button_to_actions[button];

			if (actions_mask)
			{
				for (uint64_t action_index = 0; action_index < 64; action_index++)
				{
					if (actions_mask & (1ull << action_index))
					{
						action_state_t *action_state = &actions->action_states[action_index];
						if (event->mouse_button.pressed)
						{
							action_state->pressed  |= true;
						}
						else
						{
							action_state->released |= true;
						}
						action_state->held = event->mouse_button.pressed;
					}
				}
			}
		}
	}
}

void suppress_actions(bool suppress)
{
	action_system_t *actions = get_action_system();
	actions->suppressed = suppress;
}

void action_system_clear_sticky_edges(void)
{
	action_system_t *actions = get_action_system();

	for_array(i, actions->action_states)
	{
		action_state_t *state = &actions->action_states[i];
		state->released = false;
		state->pressed  = false;
	}
}

bool action_pressed(action_t action)
{
	action_system_t *actions = get_action_system();

	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_pressed");

	// the pattern of checking if actions are suppressed instead of just not updating action system input is
	// because I want the actions to always still be "up to date" with your inputs... maybe... but that makes
	// it possible for then an action to be released without being pressed, so maybe that kinda sucks?
	// let's find out
	if (actions->suppressed)
	{
		return false;
	}

	return actions->action_states[action].pressed;
}

bool action_held(action_t action)
{
	action_system_t *actions = get_action_system();

	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_held");

	if (actions->suppressed)
	{
		return false;
	}

	return actions->action_states[action].held;
}

bool action_released(action_t action)
{
	action_system_t *actions = get_action_system();

	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_released");

	if (actions->suppressed)
	{
		return false;
	}

	return actions->action_states[action].released;
}

v2_t get_movement_axes(void)
{
	action_system_t *actions = get_action_system();

	if (actions->suppressed)
	{
		return make_v2(0, 0);
	}

	v2_t result = {
		actions->axis_states[ActionAxis_move_x],
		actions->axis_states[ActionAxis_move_y],
	};

	return result;
}

v2_t get_look_axes(void)
{
	action_system_t *actions = get_action_system();

	if (actions->suppressed)
	{
		return make_v2(0, 0);
	}

	v2_t result = {
		actions->axis_states[ActionAxis_look_x],
		actions->axis_states[ActionAxis_look_y],
	};

	return result;
}

v2_t get_action_mouse_p(void)
{
	action_system_t *actions = get_action_system();

	if (actions->suppressed)
	{
		return make_v2(0, 0);
	}

	return actions->mouse_p;
}

v2_t get_action_mouse_dp(void)
{
	action_system_t *actions = get_action_system();

	if (actions->suppressed)
	{
		return make_v2(0, 0);
	}

	return actions->mouse_dp;
}
