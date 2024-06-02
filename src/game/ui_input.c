void enqueue_ui_event(ui_event_queue_t *queue, ui_event_t *event)
{
	sll_push_back(queue->head, queue->tail, event);
}

void trickle_ui_input(ui_input_t *input, ui_event_queue_t *queue)
{
	input->buttons_pressed  = 0;
	input->buttons_released = 0;
	
	bool button_changed = false;

	while (queue->head)
	{
		ui_event_t *event = sll_pop(queue->head);

		switch (event->kind)
		{
			case UIEvent_mouse_move:
			{
				input->mouse_p = event->mouse_p;
			} break;

			case UIEvent_mouse_button:
			{
				// rule: enforce ordering on all mouse button changes
				if (button_changed)
				{
					break;
				}

				if (event->pressed)
				{
					// = instead of |= because due to the aggressive trickling rule, only one can be pressed in one frame
					input->buttons_pressed = event->button;
					input->buttons_held   |= event->button;

					// TODO: use enums more smartly
					switch (event->button)
					{
						case Button_left:   input->mouse_p_on_lmb = event->mouse_p; break;
						case Button_middle: input->mouse_p_on_mmb = event->mouse_p; break;
						case Button_right:  input->mouse_p_on_rmb = event->mouse_p; break;
					}
				}
				else
				{
					// = instead of |= because due to the aggressive trickling rule, only one can be released in one frame
					input->buttons_released =  event->button;
					input->buttons_held    &= ~event->button;

					// TODO: maybe clear the mouse_p_on_xxx values to something obvious like -1, -1?
				}

				button_changed = true;
			} break;

			default:
			{
				ASSERT(event->kind > UIEvent_none);
				ASSERT(event->kind < UIEvent_COUNT);

				enqueue_ui_event(&input->events, event);
			} break;
		}
	}

	if (!queue->head)
	{
		queue->tail = NULL;
	}
}
