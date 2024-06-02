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

void ingest_action_system_input(input_t *input)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	g_actions->mouse_p  = input->mouse_p;
	g_actions->mouse_dp = input->mouse_dp;

	for (platform_event_t *event = input->first_event;
		 event;
		 event = event->next)
	{
		if (event->kind == Event_key)
		{
			keycode_t keycode = event->key.keycode;
			uint64_t  actions = g_actions->key_to_actions[keycode];

			if (actions)
			{
				// TODO: bit scan forward or something, maybe. Daniel Lemire has a blog post about quickly scanning bit vectors.
				// here:      https://lemire.me/blog/2018/02/21/iterating-over-set-bits-quickly/
				// also this: https://lemire.me/blog/2018/03/08/iterating-over-set-bits-quickly-simd-edition/
				for (uint64_t action_index = 0; action_index < 64; action_index++)
				{
					if (actions & (1ull << action_index))
					{
						action_state_t *action_state = &g_actions->action_states[action_index];
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
	}
}

void suppress_actions(bool suppress)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	g_actions->suppressed = suppress;
}

void action_system_clear_sticky_edges(void)
{
	for_array(i, g_actions->action_states)
	{
		action_state_t *state = &g_actions->action_states[i];
		state->released = false;
		state->pressed  = false;
	}
}

bool action_pressed(action_t action)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_pressed");

	// the pattern of checking if actions are suppressed instead of just not updating action system input is
	// because I want the actions to always still be "up to date" with your inputs... maybe... but that makes
	// it possible for then an action to be released without being pressed, so maybe that kinda sucks?
	// let's find out
	if (g_actions->suppressed)
	{
		return false;
	}

	return g_actions->action_states[action].pressed;
}

bool action_held(action_t action)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_held");

	if (g_actions->suppressed)
	{
		return false;
	}

	return g_actions->action_states[action].held;
}

bool action_released(action_t action)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");
	ASSERT_MSG(action < Action_COUNT, "Passed invalid action to action_released");

	if (g_actions->suppressed)
	{
		return false;
	}

	return g_actions->action_states[action].released;
}

v2_t get_movement_axes(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	if (g_actions->suppressed)
	{
		return make_v2(0, 0);
	}

	v2_t result = {
		g_actions->axis_states[ActionAxis_move_x],
		g_actions->axis_states[ActionAxis_move_y],
	};

	return result;
}

v2_t get_look_axes(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	if (g_actions->suppressed)
	{
		return make_v2(0, 0);
	}

	v2_t result = {
		g_actions->axis_states[ActionAxis_look_x],
		g_actions->axis_states[ActionAxis_look_y],
	};

	return result;
}

v2_t get_action_mouse_p(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	if (g_actions->suppressed)
	{
		return make_v2(0, 0);
	}

	return g_actions->mouse_p;
}

v2_t get_action_mouse_dp(void)
{
	ASSERT_MSG(g_actions, "You need to equip an action system before calling other action related functions");

	if (g_actions->suppressed)
	{
		return make_v2(0, 0);
	}

	return g_actions->mouse_dp;
}
