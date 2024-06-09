//
// Scrollable Region
//

rect2_t ui_scrollable_region_begin_ex(ui_id_t id, rect2_t start_rect, ui_scrollable_region_flags_t flags)
{
	ui_scrollable_region_state_t *state = ui_get_state(id, NULL, ui_scrollable_region_state_t);
	state->flags      = flags;
	state->start_rect = start_rect;

	ui_push_clip_rect(start_rect);

	v4_t offset = { state->scroll_offset_x, state->scroll_offset_y };
	offset = ui_interpolate_v4(id, offset);

	state->interpolated_scroll_offset = offset.xy;
	
	rect2_t result_rect = rect2_add_offset(start_rect, offset.xy);

	if (state->scrolling_zone_y && (flags & UiScrollableRegionFlags_draw_scroll_bar))
	{
		rect2_cut_from_right(result_rect, ui_sz_pix(ui_scalar(UiScalar_scroll_tray_width)), NULL, &result_rect);
	}

	return result_rect;
}

rect2_t ui_scrollable_region_begin(ui_id_t id, rect2_t start_rect)
{
	return ui_scrollable_region_begin_ex(id, start_rect, UiScrollableRegionFlags_scroll_both);
}

void ui_scrollable_region_end(ui_id_t id, rect2_t final_rect)
{
	ui_scrollable_region_state_t *state = ui_get_state(id, NULL, ui_scrollable_region_state_t);

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
			state->scroll_offset_y = flt_clamp(state->scroll_offset_y - mouse_wheel, 0.0f, scrolling_zone_height);
		}
	}

	ui_scrollable_region_flags_t flags = state->flags;

	if (state->scrolling_zone_y && (flags & UiScrollableRegionFlags_draw_scroll_bar))
	{
		rect2_t start_rect     = state->start_rect;
		float   visible_height = rect2_height(start_rect);

		float total_content_height = start_rect.max.y - (final_rect.max.y - state->interpolated_scroll_offset.y);
		float scrolling_zone_ratio = visible_height / total_content_height;

		float scroll_offset = state->scroll_offset_y;
		float scroll_pct    = scroll_offset / scrolling_zone_height;

		rect2_t tray;
		rect2_cut_from_right(start_rect, ui_sz_pix(ui_scalar(UiScalar_scroll_tray_width)), &tray, &start_rect);

		rect2_t inner_tray = rect2_cut_margins(tray, ui_sz_pix(1.0f));
		ui_draw_rect(inner_tray, ui_color(UiColor_slider_background));

		float scrollbar_size = ui_size_to_height(inner_tray, ui_sz_pct(scrolling_zone_ratio));
		scrollbar_size = MAX(scrollbar_size, ui_scalar(UiScalar_min_scroll_bar_size));

		float scrollbar_movement = visible_height - scrollbar_size;
		float scrollbar_offset   = ui_interpolate_f32(ui_child_id(id, S("scrollbar")), scroll_pct*scrollbar_movement);

		rect2_t handle;
		rect2_cut_from_top(inner_tray, ui_sz_pix(scrollbar_offset), NULL,    &inner_tray);
		rect2_cut_from_top(inner_tray, ui_sz_pix(scrollbar_size),   &handle, &inner_tray);
		rect2_cut_from_top(inner_tray, ui_sz_pct(1.0f),             NULL,    &inner_tray);

		ui_draw_rect(handle, ui_color(UiColor_slider_foreground));

	}

	state->scrolling_zone_y = scrolling_zone_height;
}

//
// Header
//

v2_t ui_compute_header_size(string_t label)
{
	v2_t size = {
		.x = ui_text_width(ui_font(UiFont_header), label),
		.y = ui_font(UiFont_header)->height,
	};

	return size;
}

void ui_header_new(rect2_t rect, string_t label)
{
	ui_push_clip_rect(rect);

	font_t *font = ui_font(UiFont_header);
	ui_draw_text_default_alignment(font, rect, label);

	ui_pop_clip_rect();
}

//
// Label
//

v2_t ui_compute_label_size(string_t label)
{
	v2_t size = {
		.x = ui_text_width(ui_font(UiFont_default), label),
		.y = ui_font(UiFont_default)->height,
	};

	return size;
}

void ui_label_new(rect2_t rect, string_t label)
{
	ui_push_clip_rect(rect);

	font_t *font = ui_font(UiFont_default);
	ui_draw_text_label_alignment(font, rect, label);

	ui_pop_clip_rect();
}

//
// Progress Bar
//

void ui_progress_bar_new(rect2_t rect, string_t label, float progress)
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

v2_t ui_compute_button_size(string_t label)
{
	float margin = ui_scalar(UiScalar_text_margin);

	v2_t size = {
		.x = 2.0f*margin + ui_text_width(ui_font(UiFont_default), label),
		.y = 2.0f*margin + ui_font(UiFont_default)->height,
	};

	return size;
}

bool ui_button_new(rect2_t rect, string_t label)
{
	ui_id_t id = ui_id(label);

	bool result = false;

	// @UiHoverable
	ui_hoverable(id, rect);

	ui_style_color_t color_id = UiColor_button_idle;

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

			ui_clear_active();
		}

		color_id = UiColor_button_active;
	}
	else if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
		}

		color_id = UiColor_button_hot;
	}
	else if (ui_mouse_in_rect(rect))
	{
		// @UiPriority
		ui_set_next_hot(id, UI_PRIORITY_DEFAULT);
	}

	if (result)
	{
		// pow!
		ui_set_v4(id, ui_color(UiColor_button_fired));
	}

	v4_t color        = ui_color(color_id);
	v4_t color_interp = ui_interpolate_v4(id, color);

	ui_draw_rect(rect, color_interp);

	ui_push_clip_rect(rect);
	ui_draw_text_aligned(ui_font(UiFont_default), rect, label, make_v2(0.5f, 0.5f));
	ui_pop_clip_rect();

	return result;
}

//
// Checkbox
//

v2_t ui_compute_checkbox_size(void)
{
	float font_height = ui_font(UiFont_default)->height;

	v2_t size = {
		.x = font_height,
		.y = font_height,
	};

	return size;
}

bool ui_checkbox_new(rect2_t rect, bool *result_value)
{
	ui_id_t id = ui_id_pointer(result_value);

	bool result = false;

	ui_style_color_t color_id = UiColor_button_idle;

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
	else if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
		}

		color_id = UiColor_button_hot;
	}
	else if (ui_mouse_in_rect(rect))
	{
		ui_set_next_hot(id, UI_PRIORITY_DEFAULT);
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

	float h = rect2_height(rect);
	float checkbox_thickness = 0.15f;
	float checkbox_thickness_pixels = checkbox_thickness*h;

	rect2_t check_rect = rect2_shrink(rect, 1.5f*checkbox_thickness_pixels);

	// clamp roundedness to avoid the checkbox looking like a radio button and
	// transfer roundedness of outer rect to inner rect so the countours match
	float roundedness_ratio = min(0.66f, ui_roundedness_ratio(rect));
	float outer_roundedness = roundf(ui_roundedness_ratio_to_abs(rect,       roundedness_ratio));
	float inner_roundedness = roundf(ui_roundedness_ratio_to_abs(check_rect, roundedness_ratio));

	UI_Scalar(UiScalar_roundedness, outer_roundedness)
	{
		// rect2_t checkbox_shadow = rect;
		// rect = rect2_add_offset(rect, make_v2(0, hover_lift));

		// ui_draw_rect_outline(checkbox_shadow, ui_color(UiColor_widget_shadow), checkbox_thickness_pixels);
		ui_draw_rect_outline(rect, color_interp, checkbox_thickness_pixels);
	}

	if (*result_value) 
	{
		UI_Scalar(UiScalar_roundedness, inner_roundedness)
		{
			// rect2_t check_shadow = check_rect;
			// check_rect = rect2_add_offset(check_rect, make_v2(0, hover_lift));

			// ui_draw_rect(check_shadow, ui_color(UiColor_widget_shadow));
			ui_draw_rect(check_rect, color_interp);
		}
	}

	return result;
}

//
// Slider
//

fn_local void ui_slider_base(ui_id_t id, rect2_t rect, ui_slider_params_t *p)
{
	if (NEVER(!ui->initialized)) 
		return;

	ui_push_id(id);

	if (p->flags & UiSliderFlags_inc_dec_buttons)
	{
		float h = rect2_height(rect);

		rect2_t dec = rect2_cut_left (&rect, h);
		rect2_t inc = rect2_cut_right(&rect, h);

		rect = rect2_cut_margins_horizontally(rect, ui_sz_pix(ui_scalar(UiScalar_widget_margin)));

		int32_t delta = 0;

		if (ui_button_new(dec, S("-")))
		{
			delta = -1;
		}

		if (ui_button_new(inc, S("+")))
		{
			delta =  1;
		}

		switch (p->type)
		{
			case UiSlider_f32: *p->f32.v += delta*p->increment_amount; break;
			case UiSlider_i32: *p->i32.v += delta*p->increment_amount; break;
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

	ui_interaction_t interaction = ui_default_widget_behaviour(id, handle);

	v4_t color = ui_animate_colors(id, interaction, 
								   ui_color(UiColor_slider_foreground),
								   ui_color(UiColor_slider_hot),
								   ui_color(UiColor_slider_active),
								   ui_color(UiColor_slider_active));

	float hover_lift = ui_hover_lift(id);

	rect2_t shadow = handle;
	handle = rect2_add_offset(handle, make_v2(0, hover_lift));

	ui_draw_rect(body, ui_color(UiColor_slider_background));
	ui_draw_rect(shadow, ui_color(UiColor_widget_shadow));
	ui_draw_rect(handle, color);
	ui_draw_rect(right, ui_color(UiColor_slider_background));

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

	ui_draw_text_aligned(ui_font(UiFont_default), body, value_text, make_v2(0.5f, 0.5f));

	ui_pop_id();
}

bool ui_slider_new_ex(rect2_t rect, float *v, float min, float max, float granularity, ui_slider_flags_new_t flags)
{
	if (NEVER(!ui->initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	float init_v = *v;

	ui_id_t id = ui_id_pointer(v);

	// @UiHoverable
	ui_hoverable(id, rect);

	ui_slider_params_t p = {
		.type  = UiSlider_f32,
		.flags = flags,
		.f32 = {
			.granularity = granularity,
			.v           = v,
			.min         = min,
			.max         = max,
		},
	};

	ui_slider_base(id, rect, &p);
	
	return *v != init_v;
}

bool ui_slider_new(rect2_t rect, float *v, float min, float max)
{
	return ui_slider_new_ex(rect, v, min, max, 0.01f, 0);
}

bool ui_slider_int_new_ex(rect2_t rect, int32_t *v, int32_t min, int32_t max, ui_slider_flags_new_t flags)
{
	if (NEVER(!ui->initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	int32_t init_v = *v;

	ui_id_t id = ui_id_pointer(v);

	// @UiHoverable
	ui_hoverable(id, rect);

	ui_slider_params_t p = {
		.type  = UiSlider_i32,
		.flags = flags,
		.i32 = {
			.v           = v,
			.min         = min,
			.max         = max,
		},
	};

	ui_slider_base(id, rect, &p);
	
	return *v != init_v;
}

bool ui_slider_int_new(rect2_t rect, int32_t *v, int32_t min, int32_t max)
{
	return ui_slider_int_new_ex(rect, v, min, max, UiSliderFlags_inc_dec_buttons);
}

