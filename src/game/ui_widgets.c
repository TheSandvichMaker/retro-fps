//
// Fallback "Error" Widget
//

void ui_error_widget(rect2_t rect, string_t widget_name, string_t error_message)
{
	// TODO: This id is not very unique. May cause issues.
	ui_id_t id = ui_child_id(ui_id(widget_name), error_message);

	// Click on the error widget to pop into the debugger so you can figure out the callstack
	if (ui_button_pressed(UiButton_left, false))
	{
		if (ui_mouse_in_rect(rect))
		{
			DEBUG_BREAK();
		}
	}

	ui_draw_rect(rect, ui_color(UiColor_widget_error_background));

	ui_hover_tooltip(Sf("%cs: %cs (click to break into the debugger)", widget_name, error_message));
	ui_hoverable    (id, rect);

	// TODO: make it easier to quickly lay out multiple lines consecutively 
	rect2_t line0, line1;
	rect2_cut_from_top(rect, ui_sz_pix(ui_font(UiFont_default)->height), &line0, &rect);
	rect2_cut_from_top(rect, ui_sz_pix(ui_font(UiFont_default)->height), &line1, &rect);

	UI_Scalar(UiScalar_label_align_x, 0.5f)
	UI_Color (UiColor_text, make_v4(1, 0.15, 0.15, 1))
	{
		ui_label(line0, Sf("!! error: %cs !!", widget_name));
		ui_label(line1, error_message);
	}
}

//
// Scrollable Region
//

rect2_t ui_scrollable_region_begin_ex(ui_scrollable_region_t *state, rect2_t start_rect, ui_scrollable_region_flags_t flags)
{
	ui_id_t id = ui_id_pointer(state);
	ui_validate_widget(id);

	state->flags      = flags;
	state->start_rect = start_rect;

	ui_push_clip_rect(start_rect, true);

	v4_t offset = {0};
	offset.xy = state->scroll_offset;

	ui_id_t handle_id = ui_child_id(id, S("handle"));

	// GARBAGE ALERT
	if (!ui_is_active(handle_id))
	{
		offset = ui_interpolate_v4(ui_child_id(id, S("offset")), offset);
	}

	state->interpolated_scroll_offset = offset.xy;
	
	rect2_t result_rect = rect2_add_offset(start_rect, offset.xy);

	if (state->scroll_zone.y != 0.0f && (flags & UiScrollableRegionFlags_draw_scroll_bar))
	{
		rect2_cut_from_right(result_rect, ui_sz_pix(ui_scalar(UiScalar_scroll_tray_width)), NULL, &result_rect);
		rect2_cut_from_right(result_rect, ui_sz_pix(ui_scalar(UiScalar_outer_window_margin)), NULL, &result_rect);
	}

	return result_rect;
}

rect2_t ui_scrollable_region_begin(ui_scrollable_region_t *state, rect2_t start_rect)
{
	return ui_scrollable_region_begin_ex(state, start_rect, UiScrollableRegionFlags_scroll_both|UiScrollableRegionFlags_draw_scroll_bar);
}

void ui_scrollable_region_end(ui_scrollable_region_t *state, rect2_t final_rect)
{
	ui_id_t id = ui_id_pointer(state);

	ui_pop_clip_rect();

	float scrolling_zone_height = 0.0f;

	// TODO: Horizontal
	if (state->flags & UiScrollableRegionFlags_scroll_vertical)
	{
		float height = rect2_height(final_rect);

		if (height < 0.0f)
		{
			// content overflowed - we can scroll
			scrolling_zone_height = -height;
		}

		if (ui_mouse_in_rect(state->start_rect))
		{
			ui->next_hovered_scroll_region = id;
		}

		if (ui->hovered_scroll_region.value == id.value)
		{
			float mouse_wheel = ui_mouse_wheel(true);
			state->scroll_offset.y = flt_clamp(state->scroll_offset.y - mouse_wheel, 0.0f, scrolling_zone_height);
		}
	}

	ui_scrollable_region_flags_t flags = state->flags;

	if (state->scroll_zone.y && (flags & UiScrollableRegionFlags_draw_scroll_bar))
	{
		ui_id_t handle_id = ui_child_id(id, S("handle"));

		//------------------------------------------------------------------------
		// Figure out rectangles

		rect2_t start_rect     = state->start_rect;
		float   visible_height = rect2_height(start_rect);

		float total_content_height = start_rect.max.y - (final_rect.max.y - state->interpolated_scroll_offset.y);
		float scrolling_zone_ratio = visible_height / total_content_height;

		float scroll_offset = state->scroll_offset.y;
		float scroll_pct    = scroll_offset / scrolling_zone_height;

		rect2_t tray;
		rect2_cut_from_right(start_rect, ui_sz_pix(ui_scalar(UiScalar_scroll_tray_width)), &tray, &start_rect);

		rect2_t inner_tray = rect2_cut_margins(tray, ui_sz_pix(1.0f));
		ui_draw_rect(inner_tray, ui_color(UiColor_slider_background));

		float scrollbar_size = ui_size_to_height(inner_tray, ui_sz_pct(scrolling_zone_ratio));
		scrollbar_size = MAX(scrollbar_size, ui_scalar(UiScalar_min_scroll_bar_size));

		float scrollbar_movement = visible_height - scrollbar_size;
		float scrollbar_offset   = scroll_pct*scrollbar_movement;
		
		// A bit manual, I'd rather suppress the animation at the site where we're dealing with interaction
		if (!ui_is_active(handle_id))
		{
			scrollbar_offset = ui_interpolate_f32(ui_child_id(id, S("scrollbar")), scrollbar_offset);
		}

		rect2_t handle;
		{
			rect2_t cut_rect = inner_tray;
			rect2_cut_from_top(cut_rect, ui_sz_pix(scrollbar_offset), NULL,    &cut_rect);
			rect2_cut_from_top(cut_rect, ui_sz_pix(scrollbar_size),   &handle, &cut_rect);
		}

		//------------------------------------------------------------------------
		// Handle interaction

		if (ui_is_active(handle_id))
		{
			if (ui_button_released(UiButton_left, true))
			{
				ui_clear_active();
			}
			else
			{
				float actuating_height_min = inner_tray.min.y + 0.5f*scrollbar_size;
				float actuating_height_max = inner_tray.max.y - 0.5f*scrollbar_size;
				float centered_mouse_y     = ui->input.mouse_p.y - ui->drag_anchor.y;
				float mouse_bary           = flt_saturate(1.0 - (centered_mouse_y - actuating_height_min) / (actuating_height_max - actuating_height_min));

				state->scroll_offset.y = mouse_bary*scrolling_zone_height;

				// TODO: This is dodgy too - I just want to say "don't interpolate this" 
				ui_set_v4(ui_child_id(id, S("offset")), make_v4(state->scroll_offset.x, state->scroll_offset.y, 0, 0));

				//------------------------------------------------------------------------
				// Restrict and hide mouse

				float restrict_min = actuating_height_min + ui->drag_anchor.y;
				float restrict_max = actuating_height_max + ui->drag_anchor.y;

				ui->restrict_mouse_rect = rect2_min_max(
					make_v2(ui->input.mouse_p_on_lmb.x, restrict_min),
					make_v2(ui->input.mouse_p_on_lmb.x, restrict_max));

				ui->cursor = Cursor_none;
			}
		}
		else if (ui_is_hot(handle_id))
		{
			if (ui_button_pressed(UiButton_left, true))
			{
				ui_set_active(handle_id);
				ui_gain_focus(handle_id);
				ui->drag_anchor = sub(ui->input.mouse_p, rect2_center(handle));
			}
		} 

		if (ui_mouse_in_rect(handle))
		{
			ui_set_next_hot(handle_id);
		}

		//------------------------------------------------------------------------
		// Draw scrollbar

		ui_style_color_t color_id = UiColor_scrollbar_foreground;

		if (ui_is_active(handle_id))
		{
			color_id = UiColor_scrollbar_active;
		}
		else if (ui_is_hot(handle_id))
		{
			color_id = UiColor_scrollbar_hot;
		}

		v4_t color = ui_interpolate_v4(ui_child_id(id, S("handle_color")), ui_color(color_id));

		ui_push_clip_rect(inner_tray, true);
		ui_draw_rect(handle, color);

		//------------------------------------------------------------------------

		ui_pop_clip_rect();

	}

	state->scroll_zone.y = scrolling_zone_height;
}

//
// Collapsable Region
//

bool ui_collapsable_region(rect2_t rect, string_t title, bool *open)
{
	ui_header(rect, title);

	ui_id_t id = ui_id(title);
	ui_validate_widget(id);

	if (ui_mouse_in_rect(rect))
	{
		ui_set_next_hot(id);
	}

	if (ui_is_hot(id))
	{
		ui->cursor = Cursor_hand;

		if (ui_button_pressed(UiButton_left, true))
		{
			if (open)
			{
				*open = !*open;
			}
		}
	}

	return open ? *open : true;
}

//
// Header
//

void ui_header(rect2_t rect, string_t label)
{
	ui_push_clip_rect(rect, true);

	font_t *font = ui_font(UiFont_header);
	ui_draw_text_default_alignment(font, rect, label);

	ui_pop_clip_rect();
}

//
// Label
//

void ui_label(rect2_t rect, string_t label)
{
	ui_push_clip_rect(rect, true);

	rect = rect2_cut_margins(rect, ui_sz_pix(ui_scalar(UiScalar_text_margin)));

	font_t *font = ui_font(UiFont_default);
	ui_draw_text_label_alignment(font, rect, label);

	ui_pop_clip_rect();
}

//
// Progress Bar
//

void ui_progress_bar(rect2_t rect, string_t label, float progress)
{
	rect2_t filled, tray;
	rect2_cut_from_left(rect, ui_sz_pct(progress), &filled, &tray);
	
	ui_draw_rect(rect,   ui_color(UiColor_progress_bar_empty));
	ui_draw_rect(filled, ui_color(UiColor_progress_bar_filled));

	ui_draw_text_default_alignment(ui_font(UiFont_default), rect, label);
}

//
// Button
//

bool ui_button(rect2_t rect, string_t label)
{
	ui_id_t id = ui_id(label);
	ui_validate_widget(id);

	bool result = false;

	// @UiHoverable
	ui_hoverable(id, rect);

	ui_style_color_t color_id = UiColor_button_idle;

	if (ui_mouse_in_rect(rect) && !ui->disabled)
	{
		ui_set_next_hot(id);
	}

	if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
			ui_gain_focus(id);
		}

		color_id = UiColor_button_hot;
	}

	if (ui_is_active(id))
	{
		float   release_margin = ui_scalar(UiScalar_release_margin);
		rect2_t release_rect   = rect2_add_radius(rect, v2s(release_margin));

		bool will_trigger_if_released = ui_mouse_in_rect(release_rect);

		if (ui_button_released(UiButton_left, true))
		{
			if (will_trigger_if_released)
			{
				result = true;
			}

			ui_clear_active();
		}

		if (will_trigger_if_released)
		{
			color_id = UiColor_button_active;
		}
		else
		{
			color_id = UiColor_button_hot;
		}
	}

	if (result)
	{
		// pow!
		ui_set_v4(id, ui_color(UiColor_button_fired));
	}

	float hover_lift = ui_button_style_hover_lift(id);

	v4_t color        = ui_color(color_id);
	v4_t color_interp = ui_interpolate_v4(id, color);

	rect2_t shadow = rect;
	rect2_t button = rect2_add_offset(rect, make_v2(0.0f, hover_lift));

	ui_draw_rect(shadow, mul(color_interp, 0.75f));
	ui_draw_rect(button, color_interp);

	ui_push_clip_rect(button, true);
	ui_draw_text_aligned(ui_font(UiFont_default), button, label, make_v2(0.5f, 0.5f));
	ui_pop_clip_rect();

	return result;
}

//
// Checkbox
//

bool ui_checkbox(rect2_t rect, bool *result_value)
{
	ui_id_t id = ui_id_pointer(result_value);
	ui_validate_widget(id);

	bool result = false;

	ui_style_color_t color_id = UiColor_button_idle;

	if (ui_mouse_in_rect(rect))
	{
		ui_set_next_hot(id);
	}

	if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
			ui_gain_focus(id);
		}

		color_id = UiColor_button_hot;
	}

	if (ui_is_active(id))
	{
		if (ui_button_released(UiButton_left, true))
		{
			float   release_margin = 8.0f;
			rect2_t release_rect   = rect2_add_radius(rect, v2s(release_margin));

			if (ui_mouse_in_rect(release_rect))
			{
				result = true;
			}
			else
			{
				color_id = UiColor_button_active;
			}

			ui_clear_active();
		}
	}

	if (*result_value)
	{
		color_id = UiColor_button_active;
	}

	// checkbox was pressed
	if (result)
	{
		*result_value = !*result_value;

		// pow!
		ui_set_v4(id, ui_color(UiColor_button_fired));
	}

	v4_t color        = ui_color(color_id);
	v4_t color_interp = ui_interpolate_v4(id, color);

	float checkbox_thickness_pixels = 1;

	// Stop a high roundedness from making a checkbox look like a radio button
	float roundedness = flt_min(8.0f, ui_scalar(UiScalar_roundedness));

	rect2_t check_rect = rect2_shrink(rect, 2.0f*checkbox_thickness_pixels);

	UI_Scalar(UiScalar_roundedness, roundedness)
	ui_draw_rect_outline(rect, color_interp, checkbox_thickness_pixels);

	if (*result_value) 
	{
		float inner_roundedness = flt_max(0.0f, roundedness - 2.0f);

		UI_Scalar(UiScalar_roundedness, inner_roundedness)
		ui_draw_rect(check_rect, color_interp);
	}

	return result;
}

//
// Slider
//

fn_local void ui_slider_old_base(ui_id_t id, rect2_t rect, ui_slider_params_t *p)
{
	if (NEVER(!ui->initialized)) 
		return;

	ui_push_id(id);

	if (p->flags & UiSliderFlags_inc_dec_buttons)
	{
		float h = rect2_height(rect);

		rect2_t dec = rect2_cut_left (&rect, h);
		rect2_t inc = rect2_cut_right(&rect, h);

		//rect = rect2_cut_margins_horizontally(rect, ui_sz_pix(ui_scalar(UiScalar_widget_margin)));

		float roundedness = ui_scalar(UiScalar_roundedness);

		float delta = 0;
		float base_increment = p->increment_amount;

		if (ui_key_held(Key_control, false))
		{
			base_increment = p->major_increment_amount;
		}

		// @UiRoundednessHack
		UI_Scalar(UiScalar_roundedness, 0.0f)
		UI_Color (UiColor_roundedness, make_v4(0, 0, roundedness, roundedness))
		if (ui_button(dec, S("-")))
		{
			delta = -1;
		}

		// @UiRoundednessHack
		UI_Scalar(UiScalar_roundedness, 0.0f)
		UI_Color (UiColor_roundedness, make_v4(roundedness, roundedness, 0, 0))
		if (ui_button(inc, S("+")))
		{
			delta = 1;
		}

		switch (p->type)
		{
			case UiSlider_f32: *p->f32.v += delta*base_increment; break;
			case UiSlider_i32: *p->i32.v += delta*base_increment; break;
		}
	}

	float slider_w           = rect2_width(rect);
	float handle_w           = max(16.0f, slider_w*ui_scalar(UiScalar_slider_handle_ratio));
	float slider_effective_w = slider_w - handle_w;

	// TODO: This is needlessly confusing
	float relative_x = CLAMP((ui->input.mouse_p.x - ui->drag_anchor.x) - rect.min.x - 0.5f*handle_w, 0.0f, slider_effective_w);

	float t_slider = 0.0f;

	if (ui_is_active(id))
	{
		t_slider = relative_x / slider_effective_w;
		ui_set_f32(ui_child_id(id, S("t_slider")), t_slider);

		switch (p->type)
		{
			case UiSlider_f32:
			{
				float value = p->f32.min + t_slider*(p->f32.max - p->f32.min);
				*p->f32.v = p->f32.granularity*roundf(value / p->f32.granularity);
			} break;

			case UiSlider_i32:
			{
				*p->i32.v = p->i32.min + (int32_t)roundf(t_slider*(float)(p->i32.max - p->i32.min));
			} break;
		}

		float handle_range_min = rect.min.x + 0.5f*handle_w;

		ui->cursor              = Cursor_none;
		ui->cursor_reset_delay  = 1;
		ui->restrict_mouse_rect = rect2_from_min_dim(
			make_v2(ui->drag_anchor.x + handle_range_min, ui->input.mouse_p_on_lmb.y),
			make_v2(slider_effective_w, 0));
	}
	else
	{
		switch (p->type)
		{
			case UiSlider_f32:
			{
				t_slider = (*p->f32.v - p->f32.min) / (p->f32.max - p->f32.min);
			} break;

			case UiSlider_i32:
			{
				t_slider = (float)(*p->i32.v - p->i32.min) / (float)(p->i32.max - p->i32.min);
			} break;
		}

		t_slider = ui_interpolate_f32(ui_child_id(id, S("t_slider")), t_slider);
	}

	t_slider = saturate(t_slider);

	float handle_t = t_slider*slider_effective_w;

	rect2_t body   = rect;
	rect2_t left   = rect2_cut_left(&rect, handle_t);
	rect2_t handle = rect2_cut_left(&rect, handle_w);
	rect2_t right  = rect;

	(void)left;
	(void)right;

	// TODO: don't hack this logic
	if (p->flags & UiSliderFlags_inc_dec_buttons)
	{
		handle = rect2_cut_margins_horizontally(handle, ui_sz_pix(ui_scalar(UiScalar_widget_margin)));
	}

	ui_interaction_t interaction = ui_default_widget_behaviour(id, handle);

	string_t value_text = {0};

	switch (p->type)
	{
		case UiSlider_f32:
		{
			value_text = Sf("%.02f", *p->f32.v);
		} break;

		case UiSlider_i32:
		{
			value_text = Sf("%d", *p->i32.v);
		} break;
	}

	v4_t color = ui_animate_colors(id, interaction, 
		ui_color(UiColor_slider_foreground),
		ui_color(UiColor_slider_hot),
		ui_color(UiColor_slider_active),
		ui_color(UiColor_slider_active));

	if (interaction & UI_RELEASED)
	{
		ui->set_mouse_p = add(rect2_center(handle), ui->drag_anchor);
	}

	float hover_lift = ui_hover_lift(id);

	rect2_t shadow = handle;
	handle = rect2_add_offset(handle, make_v2(0, hover_lift));

	UI_Scalar(UiScalar_roundedness, 0.0f)
	{
		ui_draw_rect(body, ui_color(UiColor_slider_background));
		ui_draw_rect(right, ui_color(UiColor_slider_background));
	}

	ui_draw_rect(shadow, mul(color, 0.75f));
	ui_draw_rect(handle, color);

	ui_draw_text_aligned(ui_font(UiFont_default), body, value_text, make_v2(0.5f, 0.5f));

	ui_pop_id();
}

bool ui_slider_old_ex(rect2_t rect, float *v, float min, float max, float granularity, ui_slider_flags_t flags)
{
	if (NEVER(!ui->initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	float init_v = *v;

	ui_id_t id = ui_id_pointer(v);
	ui_validate_widget(id);

	// @UiHoverable
	ui_hoverable(id, rect);

	ui_slider_params_t p = {
		.type  = UiSlider_f32,
		.flags = flags,
		.increment_amount       = 1,
		.major_increment_amount = 5,
		.f32 = {
			.granularity = granularity,
			.v           = v,
			.min         = min,
			.max         = max,
		},
	};

	ui_slider_old_base(id, rect, &p);
	
	return *v != init_v;
}

bool ui_slider_old(rect2_t rect, float *v, float min, float max)
{
	return ui_slider_old_ex(rect, v, min, max, 0.01f, 0);
}

bool ui_slider_int_old_ex(rect2_t rect, int32_t *v, int32_t min, int32_t max, ui_slider_flags_t flags)
{
	if (NEVER(!ui->initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	int32_t init_v = *v;

	ui_id_t id = ui_id_pointer(v);
	ui_validate_widget(id);

	// @UiHoverable
	ui_hoverable(id, rect);

	ui_slider_params_t p = {
		.type  = UiSlider_i32,
		.flags = flags,
		.increment_amount       = 1,
		.major_increment_amount = 5,
		.i32 = {
			.v           = v,
			.min         = min,
			.max         = max,
		},
	};

	ui_slider_old_base(id, rect, &p);
	
	return *v != init_v;
}

bool ui_slider_int_old(rect2_t rect, int32_t *v, int32_t min, int32_t max)
{
	return ui_slider_int_old_ex(rect, v, min, max, UiSliderFlags_inc_dec_buttons);
}

//
// Slider (new)
//

typedef struct ui_slider_state_t
{
	string_storage_t(256) text_input_storage;
	dynamic_string_t      text_input;

	double value;

	union
	{
		float   cached_f32;
		int32_t cached_i32;
	};
} ui_slider_state_t;

bool ui_slider_base(rect2_t rect, const ui_slider_parameters_t *p)
{
	ui_id_t id = p->id;
	ui_validate_widget(id);

	//------------------------------------------------------------------------

	bool first_use;
	ui_slider_state_t *state = ui_get_state(id, &first_use, ui_slider_state_t);

	if (first_use)
	{
		state->text_input.capacity = string_storage_size(state->text_input_storage);
		state->text_input.data     = state->text_input_storage.data;
	}

	double old_value = 0.0f;

	if (p->v)
	{
		switch (p->type)
		{
			case UiSlider_f32: old_value =         *p->f32; break;
			case UiSlider_i32: old_value = (double)*p->i32; break;
			INVALID_DEFAULT_CASE;
		}

		if (*p->i32 != state->cached_i32)
		{
			state->value = old_value;
		}
	}
	else
	{
		state->value = 0.0;
	}

	double new_value = state->value;

	bool has_bounds = p->min != p->max;

	//------------------------------------------------------------------------
	// Interaction

	if (ui_mouse_in_rect(rect))
	{
		ui_set_next_hot(id);
	}

	ui_id_t text_input_id = ui_child_id(id, S("text_input"));

	if (ui_is_hot(id))
	{
		bool control_held = ui_key_held(Key_control, false);

		if (control_held)
		{
			ui->cursor = Cursor_text_input;
		}

		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
			ui_gain_focus(id);

			if (control_held)
			{
				ui_set_active(text_input_id);
				ui_gain_focus(text_input_id);

				state->text_input.count = 0;
			}
		}
	}

	if (ui_id_has_focus(text_input_id))
	{
		ui_push_layer();

		ui_set_next_id(text_input_id);
		ui_text_edit_result_t result = ui_text_edit_ex(rect, &state->text_input, &(ui_text_edit_params_t){
			.numeric_only = true,
			.preview_text = Sf(p->format_string, new_value),
			.align_x      = 0.0f,
		});

		if (result & UiTextEditResult_committed)
		{
			parse_float_result_t parsed = string_parse_float(state->text_input.string);

			if (parsed.is_valid)
			{
				new_value = parsed.value;

				if (p->type == UiSlider_i32)
				{
					new_value = round(new_value);
				}

				ui_set_f32(ui_child_id(id, S("display_value")), new_value);
			}
		}

		if (result & (UiTextEditResult_committed|UiTextEditResult_terminated))
		{
			ui_clear_active();
			ui_gain_focus(id);
		}

		ui_pop_layer();
	}
	else 
	{
		if (ui_is_active(id))
		{
			if (ui_button_released(UiButton_left, true))
			{
				ui_clear_active();

				new_value = p->granularity * roundf(new_value / p->granularity);
			}
			else
			{
				ui->cursor      = Cursor_none;
				ui->set_mouse_p = ui->input.mouse_p_on_lmb;

				v2_t delta = sub(ui->input.mouse_p, ui->input.mouse_p_on_lmb);

				double range                = has_bounds ? p->max - p->min : 5.0;
				double units_to_cover_range = 256.0;
				double rate                 = range / units_to_cover_range;

				if (ui_key_held(Key_shift, false))
				{
					rate *= 0.1;
				}

				new_value += rate*delta.y;
				if (has_bounds) new_value = CLAMP(new_value, p->min, p->max);

				ui_set_f32(ui_child_id(id, S("display_value")), new_value);
			}
		}

		if (ui_id_has_focus(id))
		{
			double rate = 1.0;

			if (ui_key_held(Key_shift, false))
			{
				rate *= 0.1;
			}

			if (ui_key_held(Key_control, false))
			{
				rate *= 10.0;
			}

			if (ui_key_pressed(Key_left, true))
			{
				new_value -= rate*p->granularity;
				if (has_bounds) new_value = CLAMP(new_value, p->min, p->max);
			}

			if (ui_key_pressed(Key_right, true))
			{
				new_value += rate*p->granularity;
				if (has_bounds) new_value = CLAMP(new_value, p->min, p->max);
			}
		}
	}

	//------------------------------------------------------------------------
	// Drawing

	rect2_t slider_area = rect2_cut_margins(rect, ui_sz_pix(ui_scalar(UiScalar_slider_margin) + 1.0f));

	double display_value = ui_interpolate_f32(ui_child_id(id, S("display_value")), new_value);

	ui_draw_rect(rect, ui_color(UiColor_slider_background));

	ui_push_clip_rect(rect, true);

	if (has_bounds)
	{
		float pct = (display_value - p->min) / (p->max - p->min);

		if (pct < 0.0) pct = 0.0;
		if (pct > 1.0) pct = 1.0;

		rect2_t slider_rect;
		rect2_cut_from_left(slider_area, ui_sz_pct(pct), &slider_rect, NULL);

		float inner_roundedness = flt_max(0.0f, ui_scalar(UiScalar_roundedness) - (ui_scalar(UiScalar_slider_margin) + 1));

		UI_Scalar(UiScalar_roundedness, inner_roundedness)
		ui_draw_rect(slider_rect, ui_color(UiColor_slider_foreground));
	}

	double rounded_value = p->granularity*round(new_value / p->granularity);
	ui_draw_text_aligned(ui_font(UiFont_default), slider_area, Sf(p->format_string, rounded_value), make_v2(0.5f, 0.5f));

	ui_pop_clip_rect();

	ui_style_color_t outline_color = ui_id_has_focus(id) 
		? UiColor_slider_outline_focused 
		: UiColor_slider_outline;

	ui_draw_rect_outline(rect, ui_color(outline_color), 1.0f);

	if (p->v)
	{
		if (old_value != new_value)
		{
			switch (p->type)
			{
				case UiSlider_f32: state->cached_f32 = *p->f32 = (float)        rounded_value ; break;
				case UiSlider_i32: state->cached_i32 = *p->i32 = (int32_t)round(rounded_value); break;
			}
		}
	}

	state->value = new_value;

	return new_value != old_value;
}

bool ui_slider_ex(rect2_t rect, float *v, float min, float max, float granularity)
{
	return ui_slider_base(rect, &(ui_slider_parameters_t){
		.id            = ui_id_pointer(v),
		.type          = UiSlider_f32,
		.min           = min,
		.max           = max,
		.granularity   = granularity,
		.f32           = v,
		.format_string = "%0.2f",
	});
}

bool ui_slider(rect2_t rect, float *v, float min, float max)
{
	return ui_slider_ex(rect, v, min, max, 0.01f);
}

bool ui_slider_int(rect2_t rect, int32_t *v, int32_t min, int32_t max)
{
	return ui_slider_base(rect, &(ui_slider_parameters_t){
		.id            = ui_id_pointer(v),
		.type          = UiSlider_i32,
		.min           = min,
		.max           = max,
		.granularity   = 1.0,
		.i32           = v,
		.format_string = "%g",
	});
}

bool ui_drag_float(rect2_t rect, float *v)
{
	return ui_slider_base(rect, &(ui_slider_parameters_t){
		.id            = ui_id_pointer(v),
		.type          = UiSlider_f32,
		.granularity   = 0.01f,
		.f32           = v,
		.format_string = "%0.2f",
	});
}

bool ui_draw_int(rect2_t rect, int32_t *v)
{
	return ui_slider_base(rect, &(ui_slider_parameters_t){
		.id            = ui_id_pointer(v),
		.type          = UiSlider_i32,
		.granularity   = 1.0f,
		.i32           = v,
		.format_string = "%g",
	});
}

//
// Text Edit
//

fn_local size_t ui_text_edit__find_insert_point_from_offset(prepared_glyphs_t *prep, float x)
{
	size_t result = 0;

	for (size_t i = 0; i < prep->count; i++)
	{
		prepared_glyph_t *glyph = &prep->glyphs[i];

		float p = 0.5f*(glyph->rect.min.x + glyph->rect.max.x);
		if (x >= p)
		{
			result = i + 1;
		}
		else
		{
			break;
		}
	}

	return result;
}

fn_local float ui_text_edit__get_caret_x(prepared_glyphs_t *prep, size_t index)
{
	float result = 0.0f;

	if (prep->count > 0)
	{
		if (index < prep->count)
		{
			result = prep->glyphs[index].rect.min.x;
		}
		else
		{
			result = prep->glyphs[prep->count - 1].rect.max.x;
		}
	}

	return result;
}

ui_text_edit_result_t ui_text_edit_ex(rect2_t rect, dynamic_string_t *buffer, const ui_text_edit_params_t *params)
{
	ui_text_edit_result_t result = 0;

	if (buffer->count > INT32_MAX)
	{
		ui_error_widget(rect, S("ui_text_edit"), S("Way too huge buffer passed to ui_text_edit!"));
		return result;
	}

	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

	ui_id_t id = ui_id_pointer(buffer);
	ui_validate_widget(id);

	ui_push_id(id);

	ui_hoverable(id, rect);

	ui_text_edit_state_t *state = ui_get_state(id, NULL, ui_text_edit_state_t);

	ui_interaction_t interaction = ui_default_widget_behaviour(id, rect);

	if (ui_is_hot(id))
	{
		ui->cursor = Cursor_text_input;
	}

	bool has_focus          = ui_id_has_focus(id);
	bool inserted_character = false;
	bool selection_active   = state->cursor != state->selection_start;

	if (state->cursor > (int)buffer->count)
	{
		state->cursor = (int)buffer->count;
	}

	if (has_focus)
	{
		for (ui_event_t *event = ui_iterate_events();
			 event;
			 event = ui_event_next(event))
		{
			switch (event->kind)
			{
				case UiEvent_text:
				{
					bool contains_representable_text = false;

					for (size_t i = 0; i < event->text.count; i++)
					{
						// TODO: Unicode
						char c = event->text.data[i];

						bool accept_text = params->numeric_only
								? ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'x')
								: ((c >= 32  && c <= 127));

						if (accept_text)
						{
							contains_representable_text = true;
							break;
						}
					}

					if (contains_representable_text)
					{
						int start = (int)state->selection_start;
						int end   = (int)state->cursor;

						if (start != end)
						{
							if (start > end) SWAP(int, start, end);

							int remove_count = end - start;
							dyn_string_remove_range(buffer, start, remove_count);

							state->cursor = start;

							result |= UiTextEditResult_edited;
						}

						for (size_t i = 0; i < event->text.count; i++)
						{
							// TODO: Unicode
							char c = event->text.data[i];

							bool accept_text = params->numeric_only
									? ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'x')
									: ((c >= 32  && c <= 127));

							if (accept_text)
							{
								bool success = dyn_string_insertc(buffer, (int)state->cursor, c);

								if (success)
								{
									state->cursor += 1;
									state->selection_start = state->cursor;

									inserted_character = true;

									result |= UiTextEditResult_edited;
								}
							}
						}
					}

					ui_consume_event(event);
				} break;

				case UiEvent_key:
				{
					if (event->pressed)
					{
						switch (event->keycode)
						{
							case Key_escape:
							{
								result |= UiTextEditResult_terminated;

								if (buffer->count == 0 && ui_id_has_focus(id))
								{
									ui->focused_id = UI_ID_NULL;
								}

								ui_consume_event(event);
							} break;

							case Key_return:
							{
								result |= UiTextEditResult_committed;
							} break;

							// @UiInput: dedicated UI keycodes?
							case Key_left:
							{
								if (selection_active && !event->shift)
								{
									int start = state->selection_start;
									int end   = state->cursor;

									if (start > end) SWAP(int, start, end);

									state->cursor = state->selection_start = start;
								}
								else
								{
									if (state->cursor > 0)
									{
										state->cursor -= 1;
									}

									if (!event->shift) 
									{
										state->selection_start = state->cursor;
									}
								}

								ui_consume_event(event);
							} break;

							// @UiInput: dedicated UI keycodes?
							case Key_right:
							{
								if (selection_active && !event->shift)
								{
									int start = state->selection_start;
									int end   = state->cursor;

									if (start > end) SWAP(int, start, end);

									state->cursor = state->selection_start = end;
								}
								else
								{
									state->cursor += 1;

									if (state->cursor > buffer->count) state->cursor = (int)buffer->count;

									if (!event->shift)
									{
										state->selection_start = state->cursor;
									}
								}

								ui_consume_event(event);
							} break;

							// @UiInput: dedicated UI keycodes?
							case Key_delete:
							case Key_backspace:
							{
								bool delete    = event->keycode == Key_delete;
								bool backspace = event->keycode == Key_backspace;

								int start = (int)state->selection_start;
								int end   = (int)state->cursor;

								if (start == end)
								{
									if (backspace && start > 0) start -= 1;
									if (delete                ) end   += 1;
								}

								if (start > end) SWAP(int, start, end);

								int remove_count = end - start;

								dyn_string_remove_range(buffer, start, remove_count);

								state->cursor          = start;
								state->selection_start = state->cursor;

								result |= UiTextEditResult_edited;

								ui_consume_event(event);
							} break;

							// @UiInput: dedicated UI keycodes?
							case 'A':
							{
								if (event->ctrl)
								{
									state->selection_start = 0;
									state->cursor          = (int)buffer->count;
									ui_consume_event(event);
								}
							} break;
						}
					}
				} break;
			}
		}
	}

	string_t string      = buffer->string;
	rect2_t  buffer_rect = rect2_shrink(rect, ui_scalar(UiScalar_text_margin));

	font_t *font = ui_font(UiFont_default);

	ui_draw_rect(rect, ui_color(UiColor_slider_background));

	if (string.count > 0)
	{
		ui_draw_text_aligned(font, buffer_rect, string, make_v2(params->align_x, 0.5f));
	}
	else if (params->preview_text.count > 0)
	{
		UI_Color(UiColor_text, ui_color(UiColor_text_preview))
		ui_draw_text_aligned(font, buffer_rect, params->preview_text, make_v2(params->align_x, 0.5f));
	}

	v4_t caret_color = ui_color(UiColor_text);
	caret_color.w = 0.5f + 0.25f*(float)sin(1.7 * PI64 * ui->current_time_s);

	if (has_focus)
	{
		prepared_glyphs_t prep = font_prepare_glyphs(font, temp, string);

		if (prep.count > 0)
		{
			if (interaction & UI_PRESSED)
			{
				float mouse_offset_x = ui->input.mouse_p.x - buffer_rect.min.x;
				state->cursor = (int)ui_text_edit__find_insert_point_from_offset(&prep, mouse_offset_x);
				state->selection_start = state->cursor;
			}
			else if (interaction & UI_HELD)
			{
				float mouse_offset_x = ui->input.mouse_p.x - buffer_rect.min.x;
				state->cursor = (int)ui_text_edit__find_insert_point_from_offset(&prep, mouse_offset_x);
			}

			size_t selection_min  = MIN(state->selection_start, state->cursor);
			size_t selection_max  = MAX(state->selection_start, state->cursor);
			size_t selection_size = selection_max - selection_min;

			float cursor_p = buffer_rect.min.x + ui_text_edit__get_caret_x(&prep, state->cursor);

			float selection_start_p = buffer_rect.min.x + ui_text_edit__get_caret_x(&prep, state->selection_start);
			float selection_end_p   = cursor_p;

			if (selection_start_p > selection_end_p)
			{
				SWAP(float, selection_start_p, selection_end_p);
			}

			rect2_t cursor_rect = rect2_from_min_dim(make_v2(cursor_p - 1.0f, buffer_rect.min.y + 2.0f), make_v2(2.0f, rect2_height(buffer_rect) - 4.0f));
			rect2_t selection_rect = rect2_min_max(make_v2(selection_start_p, buffer_rect.min.y), make_v2(selection_end_p, buffer_rect.max.y));

			if (selection_size == 0)
			{
				ui_draw_rect_roundedness(cursor_rect, caret_color, v4_from_scalar(0.0f));
			}

			ui_draw_rect(selection_rect, ui_color(UiColor_text_selection));
		}
		else
		{
			rect2_t caret_rect = rect2_from_min_dim(
				make_v2(buffer_rect.min.x - 1.0f, buffer_rect.min.y + 2.0f),
				make_v2(2.0f, rect2_height(buffer_rect) - 4.0f));

			ui_draw_rect_roundedness(caret_rect, caret_color, v4_from_scalar(0.0f));
		}
	}

	ui_pop_id();

	m_scope_end(temp);

	return result;
}

ui_text_edit_result_t ui_text_edit(rect2_t rect, dynamic_string_t *buffer)
{
	ui_text_edit_params_t params = {
		.align_x = 0.0f,
	};
	return ui_text_edit_ex(rect, buffer, &params);
}


//
// Color Picker
//

void ui_hue_picker(rect2_t rect, float *hue)
{
	if (!hue)
	{
		ui_error_widget(rect, S("ui_hue_picker"), S("no hue passed"));
		return;
	}

	ui_id_t id = ui_id_pointer(hue);
	ui_validate_widget(id);

	if (ui_mouse_in_rect(rect))
	{
		ui_set_next_hot(id);
	}

	if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
			ui_gain_focus(id);
		}
	}

	if (ui_is_active(id))
	{
		if (ui_button_released(UiButton_left, true))
		{
			ui_clear_active();
		}
		else
		{
			v2_t bary = bary_from_rect2(rect, ui->input.mouse_p);
			*hue = flt_saturate(bary.y);

			ui->cursor              = Cursor_none;
			ui->cursor_reset_delay  = 1;
			ui->restrict_mouse_rect = rect;
		}
	}

	uint32_t color_packed = 0xFFFFFFFF;

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.tex_coords  = {
			.min = { 0, 0 },
			.max = { 1, 1 },
		},
		.roundedness = add(v4_from_scalar(ui_scalar(UiScalar_roundedness)), ui_color(UiColor_roundedness)),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
		.flags       = R_UI_RECT_HUE_PICKER,
	});

	float indicator_offset = floorf(rect.min.y + (*hue) * (rect.max.y - rect.min.y));

	rect2_t indicator_rect_background = {
		.min = { rect.min.x, indicator_offset - 2 },
		.max = { rect.max.x, indicator_offset + 1 }, // WHY MAX.X -1
	};

	// UI_Scalar(UiScalar_roundedness, 0.0f)
	ui_draw_rect(indicator_rect_background, ui_color(UiColor_widget_shadow));

	rect2_t indicator_rect = {
		.min = { rect.min.x, indicator_offset - 1 },
		.max = { rect.max.x, indicator_offset     }, // WHY MAX.X -1
	};

	// UI_Scalar(UiScalar_roundedness, 0.0f)
	ui_draw_rect(indicator_rect, make_v4(1, 1, 1, 1));
}

void ui_sat_val_picker(rect2_t rect, float hue, float *sat, float *val)
{
	if (!sat || !val)
	{
		ui_error_widget(rect, S("ui_sat_val_picker"), S("Missing saturation and/or value"));
		return;
	}

	ui_id_t id = ui_combine_ids(ui_id_pointer(sat), ui_id_pointer(val));
	ui_validate_widget(id);

	if (ui_mouse_in_rect(rect))
	{
		ui_set_next_hot(id);
	}

	if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
			ui_gain_focus(id);
		}
	}

	if (ui_is_active(id))
	{
		v2_t bary = bary_from_rect2(rect, ui->input.mouse_p);
		*sat = flt_saturate(bary.x);
		*val = flt_saturate(bary.y);

		if (ui_button_released(UiButton_left, true))
		{
			ui_clear_active();
		}
		else
		{
			ui->cursor             = Cursor_none;
			ui->cursor_reset_delay = 1;
			ui->restrict_mouse_rect = rect;
		}
	}

	uint32_t color_packed = pack_color(make_v4(hue, 0, 0, 1));

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.tex_coords  = {
			.min = { 0, 0 },
			.max = { 1, 1 },
		},
		.roundedness = add(v4_from_scalar(ui_scalar(UiScalar_roundedness)), ui_color(UiColor_roundedness)),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
		.flags       = R_UI_RECT_SAT_VAL_PICKER,
	});

	float indicator_size = 8.0f;

	if (ui_is_active(id))
	{
		indicator_size = 12.0f;
	}

	indicator_size = ui_interpolate_f32(id, indicator_size);

	v2_t    indicator_p = rect2_point_from_bary(rect, make_v2(*sat, *val));
	rect2_t indicator   = rect2_center_dim(indicator_p, make_v2(indicator_size, indicator_size));

	UI_Scalar(UiScalar_roundedness, 0.5f*indicator_size)
	{
		ui_draw_rect_outline(rect2_add_radius(indicator, make_v2(1, 1)), ui_color(UiColor_widget_shadow), 2.5f);
		ui_draw_rect_outline(indicator, make_v4(1, 1, 1, 1), 1.5f);
	}
}

void ui_color_picker(rect2_t rect, v4_t *color)
{
	(void)rect;
	(void)color;
/*
	uint32_t color_packed = 0xFFFFFFFF;

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.tex_coords  = {
			.min = { 0, 0 },
			.max = { 1, 1 },
		},
		.roundedness = add(v4_from_scalar(ui_scalar(UiScalar_roundedness)), ui_color(UiColor_roundedness)),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
		.flags       = R_UI_RECT_HUE_PICKER,
	});
*/
}
