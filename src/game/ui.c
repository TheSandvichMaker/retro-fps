// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

CVAR_BOOL(cvar_ui_show_restrict_rects, "ui.show_restrict_rects", false);
CVAR_BOOL(cvar_ui_never_hide_cursor,   "ui.never_hide_cursor",   false);
CVAR_BOOL(cvar_ui_disable_animations,  "ui.disable_animations",  false);
CVAR_F32(cvar_dummy_f32, "dummy_f32", 4.0f);
CVAR_I32(cvar_dummy_i32, "dummy_i32", 2);
CVAR_STRING(cvar_dummy_string, "dummy_string", "test");

//
// IDs
//

ui_id_t ui_id(string_t string)
{
	ui_id_t parent = UI_ID_NULL;

	if (!stack_empty(ui->id_stack))
	{
		parent = stack_top(ui->id_stack);
	}

	return ui_child_id(parent, string);
}

ui_id_t ui_combine_ids(ui_id_t a, ui_id_t b)
{

	ui_id_t result = {
		hash_combine(a.value, b.value),
	};
	UI_ID_APPEND_NAME(result, string_from_storage(a._name));
	UI_ID_APPEND_NAME(result, S("^"));
	UI_ID_APPEND_NAME(result, string_from_storage(b._name));
	return result;
}

ui_id_t ui_child_id(ui_id_t parent, string_t string)
{
	ui_id_t result = {0};

	if (ui->next_id.value)
	{
		result = ui->next_id;
		ui->next_id = UI_ID_NULL;
	}
	else
	{
		uint64_t hash = string_hash(string);

		if (hash == 0)
			hash = ~(uint64_t)0;

		result = ui_combine_ids(parent, (ui_id_t){hash});

		UI_ID_APPEND_NAME(result, string);
	}

	return result;
}

ui_id_t ui_id_pointer_(void *pointer UI_ID_NAME_PARAM)
{
	ui_id_t result = { (uint64_t)(uintptr_t)pointer };

	if (ui->next_id.value)
	{
		result = ui->next_id;
		ui->next_id = UI_ID_NULL;
	}

	if (result.value)
	{
		UI_ID_SET_NAME(result, name);
	}

	ui_id_t parent = UI_ID_NULL;

	if (!stack_empty(ui->id_stack))
	{
		parent = stack_top(ui->id_stack);
	}

	return ui_combine_ids(parent, result);
}

ui_id_t ui_id_u64_(uint64_t u64 UI_ID_NAME_PARAM)
{
	ui_id_t result = { u64 };

	if (ui->next_id.value)
	{
		result = ui->next_id;
		ui->next_id = UI_ID_NULL;
	}

	if (result.value)
	{
		UI_ID_SET_NAME(result, name);
	}

	ui_id_t parent = UI_ID_NULL;

	if (!stack_empty(ui->id_stack))
	{
		parent = stack_top(ui->id_stack);
	}

	return ui_combine_ids(parent, result);
}

void ui_set_next_id(ui_id_t id)
{
	ui->next_id = id;
}

void ui_push_id(ui_id_t id)
{
	stack_push(ui->id_stack, id);
}

void ui_pop_id(void)
{
	stack_pop(ui->id_stack);
}

void ui_validate_widget_(ui_id_t id, const char *description)
{
	(void)id;
	(void)description;

#if DREAM_SLOW
	uint64_t *state;
	if (table_find_entry(&ui->widget_validation_table, id.value, &state))
	{
		if (ui->frame_index == *state)
		{
			log(UI, Error, "Widget Validation Failed: Widget was drawn with the same ID more than once this frame. Widget description: %s", description);
		}
		else if (ui->frame_index < *state)
		{
			log(UI, Error, "Widget Validation Failed: Widget validation state looks corrupt, because it looks like the widget was drawn in the future. Widget description: %s", description);
		}

		*state = ui->frame_index;
	}
	else
	{
		table_insert(&ui->widget_validation_table, id.value, ui->frame_index);
	}
#endif
}

//
// Input
//

void ui__enqueue_event(ui_event_queue_t *queue, ui_event_t *event)
{
	sll_push_back(queue->head, queue->tail, event);
}

void ui__trickle_input(ui_input_t *input, ui_event_queue_t *queue)
{
	input->buttons_pressed  = 0;
	input->buttons_released = 0;
	
	bool button_changed = false;

	while (queue->head)
	{
		ui_event_t *event = queue->head;

		switch (event->kind)
		{
			case UiEvent_mouse_move:
			{
				input->mouse_p = event->mouse_p;
			} break;

			case UiEvent_mouse_wheel:
			{
				input->mouse_wheel += event->mouse_wheel;
			} break;

			case UiEvent_mouse_button:
			{
				// rule: enforce ordering on all mouse button changes
				if (button_changed)
				{
					goto loop_exit;
				}

				if (event->pressed)
				{
					// = instead of |= because due to the aggressive trickling rule, only one can be pressed in one frame
					input->buttons_pressed = event->button;
					input->buttons_held   |= event->button;

					// TODO: use enums more smartly
					switch (event->button)
					{
						case UiButton_left:   input->mouse_p_on_lmb = event->mouse_p; break;
						case UiButton_middle: input->mouse_p_on_mmb = event->mouse_p; break;
						case UiButton_right:  input->mouse_p_on_rmb = event->mouse_p; break;
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

			// TODO: enforce ordering on keydown, keyup of the same key in the same frame

			case UiEvent_key:
			{
				input->keys_held[event->keycode] = event->pressed;

				ui__enqueue_event(&input->events, event);
			} break;

			default:
			{
				ASSERT(event->kind > UiEvent_none);
				ASSERT(event->kind < UiEvent_count);

				ui__enqueue_event(&input->events, event);
			} break;
		}

		queue->head = event->next;
	}
loop_exit:

	if (!queue->head)
	{
		queue->tail = NULL;
	}
}

ui_event_t *ui_iterate_events(void)
{
	ui_event_t *event = ui->input.events.head;

	while (event && event->consumed)
	{
		event = event->next;
	}

	return event;
}

ui_event_t *ui_event_next(ui_event_t *event)
{
	if (event)
	{
		event = event->next;

		while (event && event->consumed)
		{
			event = event->next;
		}
	}

	return event;
}

void ui_consume_event(ui_event_t *event)
{
	ASSERT(event);
	DEBUG_ASSERT_MSG(!event->consumed, "Tried to consume an event twice, that smells like a bug");

	event->consumed = true;
}

void ui_push_input_event(const ui_event_t *in_event)
{
	if (!ui->first_free_event)
	{
		ui->first_free_event = m_alloc_struct_nozero(&ui->arena, ui_event_t);
		ui->first_free_event->next = NULL;
	}

	ui_event_t *event = sll_pop(ui->first_free_event);
	copy_struct(event, in_event);

	ui__enqueue_event(&ui->queued_input, event);
}

fn_local bool ui__check_for_key(keycode_t key, bool pressed, bool consume)
{
	bool result = false;

	// Maybe iterating events all the time is bad, but actually it's probably super fine. There's going to be
	// a very small amount of these per frame
	for (ui_event_t *event = ui_iterate_events();
		 event;
		 event = ui_event_next(event))
	{
		if (event->kind    == UiEvent_key &&
			event->keycode == key         &&
			event->pressed == pressed)
		{
			result = true;

			if (consume)
			{
				ui_consume_event(event);
			}
		}
	}

	return result;
}

bool ui_key_pressed(keycode_t key, bool consume)
{
	return ui__check_for_key(key, true, consume);
}

bool ui_key_released(keycode_t key, bool consume)
{
	return ui__check_for_key(key, false, consume);
}

bool ui_key_held(keycode_t key, bool consume)
{
	bool result = ui->input.keys_held[key];

	if (consume)
	{
		ui->input.keys_held[key] = false;
	}

	return result;
}

bool ui_button_pressed(ui_buttons_t button, bool consume)
{
	bool result = !!(ui->input.buttons_pressed & button);

	if (consume)
	{
		ui->input.buttons_pressed &= ~button;
	}

	return result;
}

bool ui_button_released(ui_buttons_t button, bool consume)
{
	bool result = !!(ui->input.buttons_released & button);

	if (consume)
	{
		ui->input.buttons_released &= ~button;
	}

	return result;
}

bool ui_button_held(ui_buttons_t button, bool consume)
{
	bool result = !!(ui->input.buttons_held & button);

	if (consume)
	{
		ui->input.buttons_held &= ~button;
	}

	return result;
}

float ui_mouse_wheel(bool consume)
{
	float result = ui->input.mouse_wheel;

	if (consume)
	{
		ui->input.mouse_wheel = 0.0f;
	}

	return result;
}

//
// Panel
//

float ui_default_row_height(void)
{
	float margin     = 2.0f*ui_scalar(UiScalar_text_margin);
	float row_height = ui_font(UiFont_default)->height + margin;
	return row_height;
}

//
// Animations
//

ui_anim_t *ui_get_anim(ui_id_t id, v4_t target)
{
	PROFILE_BEGIN_FUNC;

	ui_anim_list_t *list = &ui->anim_list;

	ui_anim_t *result = NULL;

	bool enabled = (ui_scalar(UiScalar_animation_enabled) != 0.0f) && !cvar_read_bool(&cvar_ui_disable_animations);
	if (enabled)
	{
		ui_id_t          *restrict active_ids   = list->active_ids;
		ui_id_t          *restrict sleepy_ids   = list->sleepy_ids;
		ui_anim_t        *restrict active_anims = list->active;
		ui_anim_sleepy_t *restrict sleepy_anims = list->sleepy;

		int32_t active_count = list->active_count;
		int32_t sleepy_count = list->sleepy_count;

		for (int32_t active_index = 0; active_index < active_count; active_index++)
		{
			if (ui_id_equal(active_ids[active_index], id))
			{
				result = &active_anims[active_index];
				break;
			}
		}

		if (!result)
		{
			for (int32_t sleepy_index = 0; sleepy_index < sleepy_count; sleepy_index++)
			{
				if (ui_id_equal(sleepy_ids[sleepy_index], id))
				{
					ui_anim_sleepy_t *sleepy = &sleepy_anims[sleepy_index];
					sleepy->last_touched_frame_index = ui->frame_index;
					
					if (vlen(sub(target, sleepy->t_current)) >= ui->dt*UI_ANIM_SLEEPY_THRESHOLD)
					{
						ui_anim_t as_active = {
							.t_current = sleepy->t_current,
							.t_target  = target,
						};

						// add to active array
						int32_t active_index = active_count++;
						active_ids  [active_index] = sleepy_ids[sleepy_index];
						active_anims[active_index] = as_active;

						// remove from sleepy array
						int32_t swap_index = --sleepy_count;
						sleepy_ids  [sleepy_index] = sleepy_ids  [swap_index];
						sleepy_anims[sleepy_index] = sleepy_anims[swap_index];

						result = &active_anims[active_index];
					}
					else
					{
						result = &list->null;
						result->t_current = target;
					}

					break;
				}
			}
		}

		if (!result)
		{
			if (active_count < ARRAY_COUNT(list->active))
			{
				int32_t anim_index = active_count++;

				active_ids[anim_index] = id;
				result = &active_anims[anim_index];

				log(UI, SuperSpam, "Spawned anim %.*s", Sx(UI_ID_GET_NAME(id)));
			}
			else
			{
				result = &list->null;

				log(UI, Warning, "Ran out of space for UI anims for ID %.*s", Sx(UI_ID_GET_NAME(id)));
			}

			result->t_current  = target;
			result->t_velocity = make_v4(0, 0, 0, 0);
		}

		list->active_count = active_count;
		list->sleepy_count = sleepy_count;
	}
	else
	{
		result = &list->null;
		result->t_current = target;
	}

	result->last_touched_frame_index = ui->frame_index;
	result->length_limit             = ui_scalar(UiScalar_animation_length_limit);
	result->c_t                      = ui_scalar(UiScalar_animation_stiffness);
	result->c_v                      = ui_scalar(UiScalar_animation_dampen);
	result->t_target                 = target;

	PROFILE_END_FUNC;

	return result;
}

float ui_interpolate_f32(ui_id_t id, float target)
{
	ui_anim_t *result = ui_get_anim(id, v4_from_scalar(target));
	return result->t_current.x;
}

float ui_set_f32(ui_id_t id, float target)
{
	ui_anim_t *anim = ui_get_anim(id, v4_from_scalar(target));
	anim->t_target.x   = target;
	anim->t_current.x  = target;
	anim->t_velocity.x = 0.0f;

	return anim->t_current.x;
}

v4_t ui_interpolate_v4(ui_id_t id, v4_t target)
{
	ui_anim_t *anim = ui_get_anim(id, target);
	anim->t_target = target;

	return anim->t_current;
}

v4_t ui_set_v4(ui_id_t id, v4_t target)
{
	ui_anim_t *anim = ui_get_anim(id, target);
	anim->t_target   = target;
	anim->t_current  = target;
	anim->t_velocity = make_v4(0, 0, 0, 0);

	return anim->t_current;
}

fn_local void ui_tick_animations(ui_anim_list_t *list, float dt)
{
	PROFILE_BEGIN_FUNC;

	ui_id_t          *restrict active_ids = list->active_ids;
	ui_id_t          *restrict sleepy_ids = list->sleepy_ids;
	ui_anim_t        *restrict active_anims = list->active;
	ui_anim_sleepy_t *restrict sleepy_anims = list->sleepy;

	int32_t active_count = list->active_count;
	int32_t sleepy_count = list->sleepy_count;

	//
	// Delete stale sleepy anims
	//

	for (int32_t sleepy_index = 0; sleepy_index < sleepy_count;)
	{
		ui_anim_sleepy_t *anim = &sleepy_anims[sleepy_index];

		if (anim->last_touched_frame_index < ui->frame_index)
		{
			int32_t swap_index = --sleepy_count;
			sleepy_ids  [sleepy_index] = sleepy_ids  [swap_index];
			sleepy_anims[sleepy_index] = sleepy_anims[swap_index];
		}
		else
		{
			sleepy_index++;
		}
	}

	//
	// Process active anims
	//

	for (int32_t active_index = 0; active_index < active_count;)
	{
		ui_anim_t *anim = &active_anims[active_index];

		if (anim->last_touched_frame_index >= ui->frame_index)
		{
			float length_limit = anim->length_limit;
			float c_t = anim->c_t;
			float c_v = anim->c_v;

			v4_t target   = anim->t_target;
			v4_t current  = anim->t_current;
			v4_t velocity = anim->t_velocity;

			if (length_limit != FLT_MAX)
			{
				v4_t min = sub(target, length_limit);
				v4_t max = add(target, length_limit);
				current = v4_min(current, max);
				current = v4_max(current, min);
			}

			v4_t delta   = sub(target, current);
			v4_t accel_t = mul( c_t, delta);
			v4_t accel_v = mul(-c_v, velocity);
			v4_t accel = add(accel_t, accel_v);

			velocity = add(velocity, mul(dt, accel));
			current  = add(current,  mul(dt, velocity));

			anim->t_current  = current;
			anim->t_velocity = velocity;

			if (vlen(velocity) < dt*UI_ANIM_SLEEPY_THRESHOLD)
			{
				ui_anim_sleepy_t as_sleepy = {
					.last_touched_frame_index = ui->frame_index,
					.t_current                = target,
				};

				// add to sleepy array
				int32_t sleepy_index = sleepy_count++;
				sleepy_ids  [sleepy_index] = active_ids[active_index];
				sleepy_anims[sleepy_index] = as_sleepy;

				// remove from active array
				int32_t swap_index = --active_count;
				active_ids  [active_index] = active_ids  [swap_index];
				active_anims[active_index] = active_anims[swap_index];
			}
			else
			{
				active_index++;
			}
		}
		else
		{
			// remove from active array
			int32_t swap_index = --active_count;
			active_ids  [active_index] = active_ids  [swap_index];
			active_anims[active_index] = active_anims[swap_index];
		}
	}

	list->active_count = active_count;
	list->sleepy_count = sleepy_count;

	PROFILE_END_FUNC;
}

//
// Style Stacks
//

float ui_scalar(ui_style_scalar_t scalar)
{
	if (stack_empty(ui->style.scalars[scalar])) return ui->style.base_scalars[scalar];
	else                                       return stack_top(ui->style.scalars[scalar]);
}

void ui_push_scalar(ui_style_scalar_t scalar, float value)
{
    stack_push(ui->style.scalars[scalar], value);
}

float ui_pop_scalar(ui_style_scalar_t scalar)
{
    return stack_pop(ui->style.scalars[scalar]);
}

v4_t ui_color(ui_style_color_t color)
{
	if (stack_empty(ui->style.colors[color])) return ui->style.base_colors[color];
	else                                     return stack_top(ui->style.colors[color]);
}

void ui_push_color(ui_style_color_t color, v4_t value)
{
    stack_push(ui->style.colors[color], value);
}

v4_t ui_pop_color(ui_style_color_t color)
{
    return stack_pop(ui->style.colors[color]);
}

void ui_push_font(ui_style_font_t font_id, font_t *font)
{
	stack_push(ui->style.fonts[font_id], font);
}

font_t *ui_pop_font(ui_style_font_t font_id)
{
	return stack_pop(ui->style.fonts[font_id]);
}

font_t *ui_font(ui_style_font_t font_id)
{
	if (stack_empty(ui->style.fonts[font_id])) return ui->style.base_fonts[font_id];
	else                                       return stack_top(ui->style.fonts[font_id]);
}

//
// Draw
//

void ui_set_font_height(float size)
{
	for (size_t i = 0; i < UiFont_COUNT; i++)
	{
		if (ui->style.base_fonts[i])
		{
			destroy_font(ui->style.base_fonts[i]);
			ui->style.base_fonts[i] = NULL;
		}
	}

	font_range_t ranges[] = {
		{ .start = ' ', .end = '~' },
	};

	ui->style.base_fonts[UiFont_default] = make_font_from_memory(S("UI Font"), ui->style.font_data, ARRAY_COUNT(ranges), ranges, size);
	ui->style.base_fonts[UiFont_header ] = make_font_from_memory(S("UI Header Font"), ui->style.header_font_data, ARRAY_COUNT(ranges), ranges, 1.5f*size);
}

void ui_push_clip_rect(rect2_t rect)
{
	if (ALWAYS(!stack_full(ui->clip_rect_stack)))
	{
		r_rect2_fixed_t fixed   = rect2_to_fixed(rect);
		r_rect2_fixed_t clipped = rect2_fixed_intersect(fixed, ui_get_clip_rect_fixed());
		stack_push(ui->clip_rect_stack, clipped);
	}
}

void ui_pop_clip_rect(void)
{
	if (ALWAYS(!stack_empty(ui->clip_rect_stack)))
	{
		stack_pop(ui->clip_rect_stack);
	}
}

r_rect2_fixed_t ui_get_clip_rect_fixed(void)
{
	r_rect2_fixed_t clip_rect = {
		0, 0, UINT16_MAX, UINT16_MAX,
	};

	if (!stack_empty(ui->clip_rect_stack))
	{
		clip_rect = stack_top(ui->clip_rect_stack);
	}

	return clip_rect;
}

rect2_t ui_get_clip_rect(void)
{
	return rect2_from_fixed(ui_get_clip_rect_fixed());
}

rect2_t ui_text_op(font_t *font, v2_t p, string_t text, v4_t color, ui_text_op_t op)
{
	rect2_t result = rect2_inverted_infinity();

	uint32_t color_packed = pack_color(color);

	p.x = roundf(p.x);
	p.y = roundf(p.y);

	m_scoped_temp
	{
		prepared_glyphs_t prep = font_prepare_glyphs(font, temp, text);
		result = rect2_add_offset(prep.bounds, p);

		if (op == UI_TEXT_OP_DRAW)
		{
			for (size_t i = 0; i < prep.count; i++)
			{
				prepared_glyph_t *glyph = &prep.glyphs[i];

				rect2_t rect    = glyph->rect;
				rect2_t rect_uv = glyph->rect_uv;

				rect = rect2_add_offset(rect, p);

                ui_push_command(
					(ui_render_command_key_t){
						.layer  = ui->render_layer,
						.window = 0,
					},

					&(ui_render_command_t){
						.rect = {
							.rect       = rect,
							.tex_coords = rect_uv,
							.color_00   = color_packed,
							.color_10   = color_packed,
							.color_11   = color_packed,
							.color_01   = color_packed,
							.flags      = R_UI_RECT_BLEND_TEXT,
							.clip_rect  = ui_get_clip_rect_fixed(),
							.texture    = rhi_get_texture_srv(font->texture),
						},
					}
                );
			}
		}
	}

	return result;
}

rect2_t ui_text_bounds(font_t *font, v2_t p, string_t text)
{
	return ui_text_op(font, p, text, (v4_t){0,0,0,0}, UI_TEXT_OP_BOUNDS);
}

float ui_text_width(font_t *font, string_t text)
{
	rect2_t rect = ui_text_bounds(font, (v2_t){0, 0}, text);
	return rect2_width(rect);
}

float ui_text_height(font_t *font, string_t text)
{
	rect2_t rect = ui_text_bounds(font, (v2_t){0, 0}, text);
	return rect2_height(rect);
}

v2_t ui_text_dim(font_t *font, string_t text)
{
	rect2_t rect = ui_text_bounds(font, (v2_t){0, 0}, text);
	return rect2_dim(rect);
}

v2_t ui_text_align_p(font_t *font, rect2_t rect, string_t text, v2_t align)
{
    float text_width  = ui_text_width(font, text);
    float text_height = font->y_advance;

    float rect_width  = rect2_width(rect);
    float rect_height = rect2_height(rect);

    v2_t result = {
        .x = rect.min.x + align.x*rect_width  - align.x*text_width,
        .y = rect.min.y + align.y*rect_height - align.y*text_height - font->descent,
    };

    return result;
}

v2_t ui_text_center_p(font_t *font, rect2_t rect, string_t text)
{
	return ui_text_align_p(font, rect, text, make_v2(0.5f, 0.5f));
}

v2_t ui_text_default_align_p(font_t *font, rect2_t rect, string_t text)
{
	return ui_text_align_p(font, rect, text, make_v2(ui_scalar(UiScalar_text_align_x), ui_scalar(UiScalar_text_align_y)));
}

rect2_t ui_draw_text(font_t *font, v2_t p, string_t text)
{
	v4_t text_color   = ui_color(UiColor_text);
	v4_t shadow_color = ui_color(UiColor_text_shadow);

	shadow_color.w *= text_color.w;

	ui_text_op(font, add(p, make_v2(1.0f, -1.0f)), text, shadow_color, UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(font, p, text, text_color, UI_TEXT_OP_DRAW);
	return result;
}

rect2_t ui_draw_text_aligned(font_t *font, rect2_t rect, string_t text, v2_t align)
{
	v2_t p = ui_text_align_p(font, rect, text, align);

	v4_t text_color   = ui_color(UiColor_text);
	v4_t shadow_color = ui_color(UiColor_text_shadow);

	shadow_color.w *= text_color.w;

	ui_text_op(font, add(p, make_v2(1.0f, -1.0f)), text, shadow_color, UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(font, p, text, text_color, UI_TEXT_OP_DRAW);

	return result;
}

rect2_t ui_draw_text_default_alignment(font_t *font, rect2_t rect, string_t text)
{
	v2_t align = {
		.x = ui_scalar(UiScalar_text_align_x),
		.y = ui_scalar(UiScalar_text_align_y),
	};
	return ui_draw_text_aligned(font, rect, text, align);
}

rect2_t ui_draw_text_label_alignment(font_t *font, rect2_t rect, string_t text)
{
	v2_t align = {
		.x = ui_scalar(UiScalar_label_align_x),
		.y = ui_scalar(UiScalar_label_align_y),
	};
	return ui_draw_text_aligned(font, rect, text, align);
}

void ui_reset_render_commands(void)
{
	ui->render_commands.count = 0;
}

void ui_push_command(ui_render_command_key_t key, const ui_render_command_t *command)
{
	ui_render_command_list_t *list = &ui->render_commands;

	if (ALWAYS(list->count < list->capacity))
	{
		size_t index = list->count++;

		ASSERT(index <= UINT16_MAX);

		key.command_index = (uint16_t)index;

		list->keys    [index] = key;
		list->commands[index] = *command;
	}
}

void ui_sort_render_commands(void)
{
	radix_sort_u32(&ui->render_commands.keys[0].u32, ui->render_commands.count);
}

// TODO: remove... silly function
fn_local void ui_do_rect(r_ui_rect_t rect)
{
	rect.clip_rect = ui_get_clip_rect_fixed();

	rect2_t clip_rect    = rect2_from_fixed(rect.clip_rect);
	rect2_t clipped_rect = rect2_intersect(rect.rect, clip_rect);
	if (!rect2_is_inside_out(clipped_rect) && rect2_area(clipped_rect) > 0.0f)
	{
		v2_t  dim             = rect2_dim(rect.rect);
		float max_roundedness = mul(0.5f, min(dim.x, dim.y));

		rect.roundedness.x = min(rect.roundedness.x, max_roundedness);
		rect.roundedness.z = min(rect.roundedness.z, max_roundedness);
		rect.roundedness.y = min(rect.roundedness.y, max_roundedness);
		rect.roundedness.w = min(rect.roundedness.w, max_roundedness);

		ui_push_command(
			(ui_render_command_key_t){
				.layer  = ui->render_layer,
				.window = 0,
			},
			&(ui_render_command_t){
				.rect = rect,
			}
		);
	}
	else
	{
		ui->culled_rect_count += 1;
	}
}

void ui_draw_rect(rect2_t rect, v4_t color)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = add(v4_from_scalar(ui_scalar(UiScalar_roundedness)), ui_color(UiColor_roundedness)),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
	});
}

void ui_draw_rect_shadow(rect2_t rect, v4_t color, float shadow_amount, float shadow_radius)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect          = rect, 
		.roundedness   = v4_from_scalar(ui_scalar(UiScalar_roundedness)),
		.color_00      = color_packed,
		.color_10      = color_packed,
		.color_11      = color_packed,
		.color_01      = color_packed,
		.shadow_amount = shadow_amount,
		.shadow_radius = shadow_radius,
	});
}

void ui_draw_rect_roundedness(rect2_t rect, v4_t color, v4_t roundness)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = mul(roundness, v4_from_scalar(ui_scalar(UiScalar_roundedness))),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
	});
}

void ui_draw_rect_roundedness_shadow(rect2_t rect, v4_t color, v4_t roundness, float shadow_amount, float shadow_radius)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = mul(roundness, v4_from_scalar(ui_scalar(UiScalar_roundedness))),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
		.shadow_amount = shadow_amount,
		.shadow_radius = shadow_radius,
	});
}

void ui_draw_debug_rect(rect2_t rect, v4_t color)
{
	uint32_t color_packed = pack_color(color);

	ui_render_layer_t old_layer = ui->render_layer;
	ui->render_layer = UI_LAYER_DEBUG_OVERLAY;

	ui_do_rect((r_ui_rect_t){
		.rect         = rect, 
		.roundedness  = 0.0f,
		.color_00     = color_packed,
		.color_10     = color_packed,
		.color_11     = color_packed,
		.color_01     = color_packed,
		.inner_radius = 2.0f,
	});

	ui->render_layer = old_layer;
}

void ui_draw_rect_outline(rect2_t rect, v4_t color, float width)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect         = rect, 
		.roundedness  = v4_from_scalar(ui_scalar(UiScalar_roundedness)),
		.color_00     = color_packed,
		.color_10     = color_packed,
		.color_11     = color_packed,
		.color_01     = color_packed,
		.inner_radius = width,
	});
}

void ui_draw_rect_roundedness_outline(rect2_t rect, v4_t color, v4_t roundedness, float width)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect         = rect, 
		.roundedness  = mul(roundedness, v4_from_scalar(ui_scalar(UiScalar_roundedness))),
		.color_00     = color_packed,
		.color_10     = color_packed,
		.color_11     = color_packed,
		.color_01     = color_packed,
		.inner_radius = width,
	});
}

void ui_draw_circle(v2_t p, float radius, v4_t color)
{
	uint32_t color_packed = pack_color(color);

	rect2_t rect = rect2_center_radius(p, make_v2(radius, radius));

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = v4_from_scalar(radius),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
	});
}

//
// Widget Building Utilities
//

bool ui_mouse_in_rect(rect2_t rect)
{
	rect2_t clip_rect = ui_get_clip_rect();
	return rect2_contains_point(rect2_intersect(rect, clip_rect), ui->input.mouse_p);
}

ui_interaction_t ui_default_widget_behaviour_priority(ui_id_t id, rect2_t rect, ui_priority_t priority)
{
	uint32_t result = 0;

	bool hovered = ui_mouse_in_rect(rect);

	if (hovered)
	{
        ui_set_next_hot(id, priority);
		result |= UI_HOVERED;
	}

	if (ui_is_hot(id))
	{
		result |= UI_HOT;

		if (ui_button_pressed(UiButton_left, false))
		{
			result |= UI_PRESSED;
			ui->drag_anchor = sub(ui->input.mouse_p, rect2_center(rect));
			ui_set_active(id);
		}
	}

	if (ui_is_active(id))
	{
		result |= UI_HELD;

		if (ui_button_released(UiButton_left, false))
		{
			if (hovered)
			{
				result |= UI_FIRED;
			}

			result |= UI_RELEASED;

			ui_clear_active();
		}
	}

	return result;
}

ui_interaction_t ui_default_widget_behaviour(ui_id_t id, rect2_t rect)
{
	ui_priority_t priority = UI_PRIORITY_DEFAULT;

	if (ui->override_priority != UI_PRIORITY_DEFAULT)
	{
		priority = ui->override_priority;
	}

	return ui_default_widget_behaviour_priority(id, rect, priority);
}

float ui_roundedness_ratio(rect2_t rect)
{
	float w = rect2_width (rect);
	float h = rect2_height(rect);
	float c = 0.5f*min(w, h);
	float ratio = ui_scalar(UiScalar_roundedness) / c;
	return ratio;
}

float ui_roundedness_ratio_to_abs(rect2_t rect, float ratio)
{
	float w = rect2_width (rect);
	float h = rect2_height(rect);
	float c = 0.5f*min(w, h);
	float abs = c*ratio;
	return abs;
}

v4_t ui_animate_colors(ui_id_t id, uint32_t interaction, v4_t cold, v4_t hot, v4_t active, v4_t fired)
{
	ui_id_t color_id = ui_child_id(id, S("color"));

	if (interaction & UI_FIRED)
	{
		ui_set_v4(color_id, fired);
	}

	v4_t  target    = cold;
	float stiffness = ui_scalar(UiScalar_animation_stiffness);

	if (interaction & UI_HOT)
	{
		target = hot;
	}

	if (interaction & (UI_PRESSED|UI_HELD))
	{
		target = active;
		stiffness *= 2.0f;
	}

	v4_t interp_color;

	UI_Scalar(UiScalar_animation_stiffness, stiffness)
	interp_color = ui_interpolate_v4(color_id, target);

	return interp_color;
}

float ui_hover_lift(ui_id_t id)
{
	float hover_lift = ui_is_hot(id) ? ui_scalar(UiScalar_hover_lift) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);
	return hover_lift;
}

float ui_button_style_hover_lift(ui_id_t id)
{
	float hover_lift = ui_is_hot(id) && !ui_is_active(id) ? ui_scalar(UiScalar_hover_lift) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);
	return hover_lift;
}

void ui_tooltip(string_t text)
{
	if (!stack_full(ui->tooltip_stack))
	{
		ui_tooltip_t *tooltip = stack_add(ui->tooltip_stack);
		string_into_storage(tooltip->text, text);
	}
}

void ui_hover_tooltip(string_t text)
{
	string_into_storage(ui->next_tooltip, text);
}

//
// Core
//

void equip_ui(ui_t *state)
{
	DEBUG_ASSERT_MSG(!ui, "Unequip the previous UI state before you equip this new one, please");
	ui = state;
}

void unequip_ui(void)
{
	DEBUG_ASSERT_MSG(ui, "You should only call unequip_ui if there's actually a ui equipped");
	ui = NULL;
}

bool ui_is_cold(ui_id_t id)
{
	return (ui->hot.value != id.value && ui->active.value != id.value);
}

bool ui_is_next_hot(ui_id_t id)
{
	return ui->next_hot.value == id.value;
}

bool ui_is_hot(ui_id_t id)
{
	return ui->hot.value == id.value;
}

bool ui_is_active(ui_id_t id)
{
	return ui->active.value == id.value;
}

bool ui_is_hovered_panel(ui_id_t id)
{
	return ui->hovered_panel.value == id.value;
}

void ui_set_hot(ui_id_t id)
{
	if (!ui->active.value)
	{
		ui->hot = id;
	}
}

void ui_set_next_hot(ui_id_t id, ui_priority_t priority)
{
	if (ui->next_hot_priority <= priority)
	{
		ui->next_hot          = id;
		ui->next_hot_priority = priority;
	}
}

void ui_set_active(ui_id_t id)
{
	ui->active = id;
	ui->focused_id = id;
}

void ui_clear_next_hot(void)
{
	ui->next_hot.value = 0;
}

void ui_clear_hot(void)
{
	ui->hot.value = 0;
}

void ui_clear_active(void)
{
	ui->active.value = 0;
}

bool ui_has_focus(void)
{
	return ui->has_focus && ui->app_has_focus;
}

bool ui_id_has_focus(ui_id_t id)
{
	return ui->has_focus && ui->focused_id.value == id.value;
}

void *ui_get_state_raw(ui_id_t id, bool *first_touch, uint16_t size, ui_state_flags_t flags)
{
	ui_state_header_t *state = table_find_object(&ui->state_index, id.value);

	if (!state)
	{
		uint16_t real_size = checked_add_u16(sizeof(ui_state_header_t), size);

		state = simple_heap_alloc(&ui->state_allocator, real_size);
		state->id                  = id;
		state->size                = real_size;
		state->created_frame_index = ui->frame_index;
		state->flags               = flags;
		table_insert_object(&ui->state_index, id.value, state);

		if (first_touch) *first_touch = true;
	}
	else if (first_touch)
	{
		*first_touch = false;
	}

	ASSERT(state->id.value == id.value);

	state->last_touched_frame_index = ui->frame_index;

	void *result = state + 1;
	return result;
}

void ui_hoverable(ui_id_t id, rect2_t rect)
{
	if (ui_mouse_in_rect(rect))
	{
		ui->next_hovered_widget = id;
	}

	string_t next_tooltip = string_from_storage(ui->next_tooltip);

	if (!string_empty(next_tooltip) && 
		ui_is_hovered_delay(id, ui_scalar(UiScalar_tooltip_delay)))
	{
		ui_tooltip(next_tooltip);
	}

	ui->next_tooltip.count = 0;
}

bool ui_is_hovered(ui_id_t id)
{
	return ui->hovered_widget.value == id.value;
}

bool ui_is_hovered_delay(ui_id_t id, float delay)
{
	return ui_is_hovered(id) && (ui->hover_time_seconds >= delay);
}

static void ui_initialize(void)
{
	//------------------------------------------------------------------------
	// register console variables

	cvar_register(&cvar_ui_show_restrict_rects);
	cvar_register(&cvar_ui_never_hide_cursor);
	cvar_register(&cvar_ui_disable_animations);
	cvar_register(&cvar_dummy_f32);
	cvar_register(&cvar_dummy_i32);
	cvar_register(&cvar_dummy_string);

	//------------------------------------------------------------------------

	zero_struct(ui);
	// ui->state = (pool_t)INIT_POOL(ui_state_t);

	simple_heap_init(&ui->state_allocator, &ui->arena);

	ASSERT(!ui->initialized);

	ui->style.font_data        = fs_read_entire_file(&ui->arena, S("gamedata/fonts/NotoSans/NotoSans-Regular.ttf"));
	ui->style.header_font_data = fs_read_entire_file(&ui->arena, S("gamedata/fonts/NotoSans/NotoSans-Bold.ttf"));
	ui_set_font_height(18.0f);

	v4_t background    = make_v4(0.19f, 0.15f, 0.17f, 1.0f);
	v4_t background_hi = make_v4(0.22f, 0.18f, 0.18f, 1.0f);
	v4_t foreground    = make_v4(0.33f, 0.28f, 0.28f, 1.0f);

	v4_t hot    = make_v4(0.25f, 0.45f, 0.40f, 1.0f);
	v4_t active = make_v4(0.30f, 0.55f, 0.50f, 1.0f);
	v4_t fired  = make_v4(0.45f, 0.65f, 0.55f, 1.0f);

	ui->init_time = os_hires_time();

	ui->style.base_scalars[UiScalar_tooltip_delay         ] = 0.0f;
	ui->style.base_scalars[UiScalar_animation_enabled     ] = 1.0f;
	ui->style.base_scalars[UiScalar_animation_stiffness   ] = 512.0f;
	ui->style.base_scalars[UiScalar_animation_dampen      ] = 32.0f;
	ui->style.base_scalars[UiScalar_animation_length_limit] = FLT_MAX;
	ui->style.base_scalars[UiScalar_hover_lift            ] = 1.0f;
	ui->style.base_scalars[UiScalar_window_margin         ] = 0.0f;
	ui->style.base_scalars[UiScalar_widget_margin         ] = 2.0f;
	ui->style.base_scalars[UiScalar_row_margin            ] = 2.0f;
	ui->style.base_scalars[UiScalar_text_margin           ] = 2.0f;
	ui->style.base_scalars[UiScalar_roundedness           ] = 2.0f;
	ui->style.base_scalars[UiScalar_text_align_x          ] = 0.5f;
	ui->style.base_scalars[UiScalar_text_align_y          ] = 0.5f;
	ui->style.base_scalars[UiScalar_label_align_x         ] = 0.0f;
	ui->style.base_scalars[UiScalar_label_align_y         ] = 0.5f;
	ui->style.base_scalars[UiScalar_scroll_tray_width     ] = 16.0f;
	ui->style.base_scalars[UiScalar_outer_window_margin   ] = 4.0f;
	ui->style.base_scalars[UiScalar_min_scroll_bar_size   ] = 32.0f;
	ui->style.base_scalars[UiScalar_slider_handle_ratio   ] = 1.0f / 4.0f;
	ui->style.base_colors [UiColor_text                   ] = make_v4(0.95f, 0.90f, 0.85f, 1.0f);
	ui->style.base_colors [UiColor_text_low               ] = make_v4(0.85f, 0.80f, 0.75f, 1.0f);
	ui->style.base_colors [UiColor_text_preview           ] = mul(0.65f, ui->style.base_colors[UiColor_text]);
	ui->style.base_colors [UiColor_text_shadow            ] = make_v4(0.00f, 0.00f, 0.00f, 0.50f);
	ui->style.base_colors [UiColor_text_selection         ] = make_v4(0.12f, 0.40f, 0.32f, 0.50f);
	ui->style.base_colors [UiColor_widget_shadow          ] = make_v4(0.00f, 0.00f, 0.00f, 0.20f);
	ui->style.base_colors [UiColor_widget_error_background] = make_v4(0.50f, 0.20f, 0.20f, 0.50f);
	ui->style.base_colors [UiColor_window_background      ] = background;
	ui->style.base_colors [UiColor_window_title_bar       ] = make_v4(0.25f, 0.35f, 0.30f, 1.0f);
	ui->style.base_colors [UiColor_window_title_bar_hot   ] = make_v4(0.29f, 0.41f, 0.36f, 1.0f);
	ui->style.base_colors [UiColor_window_close_button    ] = make_v4(0.35f, 0.15f, 0.15f, 1.0f);
	ui->style.base_colors [UiColor_window_outline         ] = make_v4(0.1f, 0.1f, 0.1f, 0.20f);
	ui->style.base_colors [UiColor_region_outline         ] = make_v4(0.28f, 0.265f, 0.25f, 0.5f);
	ui->style.base_colors [UiColor_progress_bar_empty     ] = background_hi;
	ui->style.base_colors [UiColor_progress_bar_filled    ] = hot;
	ui->style.base_colors [UiColor_button_idle            ] = foreground;
	ui->style.base_colors [UiColor_button_hot             ] = hot;
	ui->style.base_colors [UiColor_button_active          ] = active;
	ui->style.base_colors [UiColor_button_fired           ] = fired;
	ui->style.base_colors [UiColor_slider_background      ] = background_hi;
	ui->style.base_colors [UiColor_slider_foreground      ] = foreground;
	ui->style.base_colors [UiColor_slider_hot             ] = hot;
	ui->style.base_colors [UiColor_slider_active          ] = active;

	ui->render_commands.capacity = UI_RENDER_COMMANDS_CAPACITY;
	ui->render_commands.keys     = m_alloc_array_nozero(&ui->arena, ui->render_commands.capacity, ui_render_command_key_t);
	ui->render_commands.commands = m_alloc_array_nozero(&ui->arena, ui->render_commands.capacity, ui_render_command_t);

	ui->initialized = true;
}

bool ui_begin(float dt, rect2_t ui_area)
{
	ASSERT(ui);
	ASSERT(ui->initialized);

	ui__trickle_input(&ui->input, &ui->queued_input);

	ui->dt = dt;
	ui->ui_area = ui_area;

	ui->last_frame_culled_rect_count = ui->culled_rect_count;
	ui->culled_rect_count = 0;
	ui->last_frame_ui_rect_count = ui->render_commands.count;
    ui_reset_render_commands();

	for (table_iter_t it = table_iter(&ui->state_index); table_iter_next(&it);)
	{
		ui_state_header_t *state = it.ptr;

		if (!(state->flags & UiStateFlag_persistent) &&
			state->last_touched_frame_index < ui->frame_index)
		{
			table_remove(&ui->state_index, state->id.value);
			simple_heap_free(&ui->state_allocator, state, state->size);
		}
	}

	ui_tick_animations(&ui->anim_list, dt);

	ui->frame_index += 1;
	
	// now that the frame index is incremented, clear the current frame arena
	arena_t *frame_arena = ui_frame_arena();
	m_reset_and_decommit(frame_arena);

	ui->current_time   = os_hires_time();
	ui->current_time_s = os_seconds_elapsed(ui->init_time, ui->current_time);

    if (!ui->active.value)
	{
		if (ui->next_hovered_widget.value &&
			ui->next_hovered_widget.value != ui->hovered_widget.value)
		{
			ui->hover_time = ui->current_time;
		}

		ui->hot                   = ui->next_hot;
		ui->hovered_panel         = ui->next_hovered_panel;
		ui->hovered_widget        = ui->next_hovered_widget;
		ui->hovered_scroll_region = ui->next_hovered_scroll_region;
	}

	ui->hover_time_seconds = 0.0f;

	if (ui->hovered_widget.value)
	{
		ui->hover_time_seconds = (float)os_seconds_elapsed(ui->hover_time, ui->current_time);
	}

	ui->next_hot                   = UI_ID_NULL;
	ui->next_hot_priority          = UI_PRIORITY_DEFAULT;
	ui->next_hovered_panel         = UI_ID_NULL;
	ui->next_hovered_widget        = UI_ID_NULL;
	ui->next_hovered_scroll_region = UI_ID_NULL;

	ui->hovered = false;
	
	if (ui->cursor_reset_delay > 0)
	{
		ui->cursor_reset_delay -= 1;
	}
	else
	{
		ui->cursor = Cursor_arrow;
	}

	ui->set_mouse_p = make_v2(-1, -1);
	ui->restrict_mouse_rect = rect2_inverted_infinity();

	return ui->has_focus;
}

void ui_end(void)
{
#if 0
    int res_x, res_y;
    render->get_resolution(&res_x, &res_y);
#endif
	int res_x = 1920;
	int res_y = 1080;

	float font_height = ui_font(UiFont_default)->height;
	float at_y = 32.0f;

	for (size_t i = 0; i < stack_count(ui->tooltip_stack); i++)
	{
		ui_tooltip_t *tooltip = &ui->tooltip_stack.values[i];

		string_t text = string_from_storage(tooltip->text);

		float text_w = ui_text_width(ui_font(UiFont_default), text);

		float center_x = 0.5f*(float)res_x;
		rect2_t rect = rect2_center_dim(make_v2(center_x, at_y), make_v2(4.0f + text_w, font_height));

		ui_draw_rect_shadow(rect, make_v4(0, 0, 0, 0.5f), 0.20f, 32.0f);
		ui_draw_text_aligned(ui_font(UiFont_default), rect, text, make_v2(0.5f, 0.5f));
		at_y += ui_font(UiFont_default)->y_advance;
	}

	stack_reset(ui->tooltip_stack);

	debug_notif_t *notifs           = ui->first_debug_notif;
	debug_notif_t *surviving_notifs = NULL;

	v2_t notif_at = { 4, res_y - 4 };

	ui->first_debug_notif = NULL;
	while (notifs)
	{
		debug_notif_t *notif = sll_pop(notifs);

		v2_t text_dim = ui_text_dim(ui_font(UiFont_default), notif->text);

		v2_t pos = add(notif_at, make_v2(0, -text_dim.y));

		float lifetime_t = notif->lifetime / notif->init_lifetime;

		v4_t color = notif->color;
		color.w *= smoothstep(saturate(4.0f * lifetime_t));

		UI_Color(UiColor_text, color)
		ui_draw_text(ui_font(UiFont_default), pos, notif->text);

		notif_at.y -= text_dim.y + 4.0;

		notif->lifetime -= ui->dt;
		if (notif->lifetime > 0.0f)
		{
			sll_push(surviving_notifs, notif);
		}
	}

	while (surviving_notifs)
	{
		debug_notif_t *notif = sll_pop(surviving_notifs);
		debug_notif_replicate(notif);
	}

	if (ui_button_pressed(UiButton_any, true))
	{
		ui->has_focus = ui->hovered;
	}

	if (ui->has_focus && ui_key_pressed(Key_escape, true))
	{
		if (ui_id_valid(ui->focused_id))
		{
			ui->focused_id = UI_ID_NULL;
		}
		else
		{
			ui->has_focus = false;
		}
	}

	ASSERT(ui->id_stack.at == 0);

	ui_sort_render_commands();

	if (ui->input.events.head)
	{
		ASSERT(ui->input.events.tail);

		ui->input.events.tail->next = ui->first_free_event;
		ui->first_free_event = ui->input.events.head;
		ui->input.events.head = NULL;
		ui->input.events.tail = NULL;
	}

	if (cvar_read_bool(&cvar_ui_show_restrict_rects))
	{
		if (!rect2_is_inside_out(ui->restrict_mouse_rect))
		{
			rect2_t rect = ui->restrict_mouse_rect;

			if (rect2_width(rect) == 0.0f)
			{
				rect = rect2_add_right(rect, 1.0f);
			}

			if (rect2_height(rect) == 0.0f)
			{
				rect = rect2_add_bottom(rect, 1.0f);
			}

			ui_draw_debug_rect(rect, COLORF_YELLOW);
		}
	}

	if (cvar_read_bool(&cvar_ui_never_hide_cursor))
	{
		if (ui->cursor == Cursor_none)
		{
			ui->cursor = Cursor_arrow;
		}
	}
}

void debug_text(v4_t color, string_t fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	debug_text_va(color, fmt, args);
	va_end(args);
}

void debug_text_va(v4_t color, string_t fmt, va_list args)
{
	debug_notif_va(color, 0.0f, fmt, args);
}

void debug_notif(v4_t color, float time, string_t fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	debug_notif_va(color, time, fmt, args);
	va_end(args);
}

void debug_notif_va(v4_t color, float time, string_t fmt, va_list args)
{
	m_scoped_temp
	{
		string_t text = string_format(ui_frame_arena(), string_null_terminate(temp, fmt).data, args);

		debug_notif_t *notif = m_alloc_struct(ui_frame_arena(), debug_notif_t);
		notif->id            = ui_id_u64(ui->next_notif_id++);
		notif->color         = color;
		notif->text          = text;
		notif->init_lifetime = time;
		notif->lifetime      = time;

		notif->next = ui->first_debug_notif;
		ui->first_debug_notif = notif;
	}
}

void debug_notif_replicate(debug_notif_t *notif)
{
	debug_notif_t *repl = m_copy_struct(ui_frame_arena(), notif);
	repl->text = m_copy_string(ui_frame_arena(), repl->text);

	repl->next = ui->first_debug_notif;
	ui->first_debug_notif = repl;
}
