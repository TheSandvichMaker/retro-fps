ui_row_builder_t ui_make_row_builder_ex(rect2_t rect, const ui_row_builder_params_t *params)
{
	ui_row_builder_t result = {
		.rect  = rect,
		.flags = params->flags,
	};

	return result;
}

ui_row_builder_t ui_make_row_builder(rect2_t rect)
{
	ui_row_builder_params_t params = {
		.flags = UiRowBuilder_checkbox_on_left,
	};

	return ui_make_row_builder_ex(rect, &params);
}

rect2_t ui_row_ex(ui_row_builder_t *builder, float height, bool draw_background)
{
	if (builder->row_index > 0 && (builder->flags & UiRowBuilder_insert_row_separators))
	{
		ui_row_separator(builder);
	}

	float margin = ui_scalar(UiScalar_row_margin);
	float bot_margin = ceilf(0.5f*margin);
	float top_margin = margin - bot_margin;

	rect2_t row;
	rect2_cut_from_top(builder->rect, ui_sz_pix(height + margin), &row, &builder->rect);

	if (draw_background)
	{
		UI_Scalar(UiScalar_roundedness, 0.0f)
		ui_draw_rect_shadow(row, make_v4(0, 0, 0, 0.02f), 0.2f, 2.0f);
	}

	rect2_t result = row;
	rect2_cut_from_bottom(result, ui_sz_pix(bot_margin), NULL, &result);
	rect2_cut_from_top   (result, ui_sz_pix(top_margin), NULL, &result);

	builder->row_index += 1;
	builder->last_row = result;

	return result;
}

rect2_t ui_row(ui_row_builder_t *builder)
{
	float height = ui_default_row_height();
	return ui_row_ex(builder, height, false);
}

void ui_row_split(ui_row_builder_t *builder, rect2_t *label, rect2_t *widget)
{
	rect2_t row = ui_row(builder);
	rect2_cut_from_left(row, ui_sz_pct(0.5f), label, widget);
}

void ui_row_spacer(ui_row_builder_t *builder, ui_size_t size)
{
	rect2_cut_from_top(builder->rect, size, NULL, &builder->rect);
}

void ui_row_separator(ui_row_builder_t *builder)
{
	ui_row_spacer(builder, ui_sz_pix(ui_scalar(UiScalar_row_margin)));

	rect2_t separator_rect;
	rect2_cut_from_top(builder->rect, ui_sz_pix(1.0f), &separator_rect, &builder->rect);

	ui_draw_rect(separator_rect, ui_color(UiColor_region_outline));

	ui_row_spacer(builder, ui_sz_pix(ui_scalar(UiScalar_row_margin)));
}

void ui_row_error_widget(ui_row_builder_t *builder, string_t widget_name, string_t error_message)
{
	float height = 2.0f*ui_default_row_height();
	rect2_t row = ui_row_ex(builder, height, false);
	ui_error_widget(row, widget_name, error_message);
}

bool ui_row_collapsable_region(ui_row_builder_t *builder, string_t label, bool *open)
{
	float height = ui_font(UiFont_header)->height;

	rect2_t row = ui_row_ex(builder, height, true);
	return ui_collapsable_region(row, label, open);
}

void ui_row_header(ui_row_builder_t *builder, string_t label)
{
	float height = ui_font(UiFont_header)->height;

	rect2_t row = ui_row_ex(builder, height, true);
	ui_header(row, label);
}

void ui_row_label(ui_row_builder_t *builder, string_t label)
{
	rect2_t rect = ui_row(builder);
	ui_label(rect, label);
}

void ui_row_progress_bar(ui_row_builder_t *builder, string_t label, float progress)
{
	float height = 0.75f*ui_default_row_height();

	rect2_t rect = ui_row_ex(builder, height, false);
	ui_progress_bar(rect, label, progress);
}

bool ui_row_button(ui_row_builder_t *builder, string_t label)
{
	rect2_t rect = ui_row(builder);
	return ui_button(rect, label);
}

bool ui_row_radio_buttons(ui_row_builder_t *builder, string_t label, int *state, string_t *option_labels, int option_count)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);

	int current_state = state ? *state : 0;
	int new_state = current_state;

	ui_id_t id = ui_id(label);
	ui_validate_widget(id);

	ui_gain_tab_focus(id);

	int keyboard_activated = -1;

	bool responder = ui_in_responder_chain(id);

	if (responder)
	{
		if (ui_key_pressed(Key_left, true))
		{
			if (ui->selection_index > 0)
			{
				ui->selection_index -= 1;
			}
		}

		if (ui_key_pressed(Key_right, true))
		{
			if (ui->selection_index < option_count - 1)
			{
				ui->selection_index += 1;
			}
		}

		if (ui_key_pressed(Key_return, true) ||
			ui_key_pressed(Key_space, true))
		{
			keyboard_activated = ui->selection_index;
		}
	}

	ui_push_responder_stack(id);

	if (option_count > 0)
	{
		float margin       = ui_scalar(UiScalar_widget_margin);
		float margin_space = (float)(option_count - 1)*margin;
		float button_width = (rect2_width(widget_rect) - margin_space) / (float)option_count;

		for (int i = 0; i < option_count; i++)
		{
			bool is_first = i == 0;
			bool is_last  = i == option_count - 1;

			v4_t roundedness = {
				is_last  ? ui_scalar(UiScalar_roundedness) : 0.0f,
				is_last  ? ui_scalar(UiScalar_roundedness) : 0.0f,
				is_first ? ui_scalar(UiScalar_roundedness) : 0.0f,
				is_first ? ui_scalar(UiScalar_roundedness) : 0.0f,
			};

			rect2_t button_rect;
			rect2_cut_from_left(widget_rect, ui_sz_pix(button_width), &button_rect, &widget_rect);
			rect2_cut_from_left(widget_rect, ui_sz_pix(margin), NULL, &widget_rect);

			bool active = (current_state == i);

			ui_suppress_next_tab_focus();

			UI_Scalar          (UiScalar_roundedness, 0.0f)
			UI_Color           (UiColor_roundedness, roundedness)
			UI_ColorConditional(UiColor_button_idle, ui_color(UiColor_button_active), active)
			if (ui_button(button_rect, option_labels[i]) || (keyboard_activated == i))
			{
				new_state = i;
				ui->selection_index = i;
			}

			if (responder && i == ui->selection_index)
			{
				ui_draw_focus_indicator(button_rect);
			}
		}
	}

	ui_pop_responder_stack();

	if (state)
	{
		*state = new_state;
	}

	bool result = current_state != new_state;
	return result;
}

bool ui_row_checkbox(ui_row_builder_t *builder, string_t label, bool *v)
{
	if (builder->flags & UiRowBuilder_checkbox_on_left)
	{
		rect2_t row = ui_row(builder);

		rect2_t checkbox_rect;
		rect2_cut_from_left(row, ui_sz_aspect(1.0f),                           &checkbox_rect, &row);
		rect2_cut_from_left(row, ui_sz_pix(ui_scalar(UiScalar_widget_margin)), NULL,           &row);

		checkbox_rect = rect2_cut_margins(checkbox_rect, ui_sz_pix(2.0f));

		ui_label(row, label);
		return ui_checkbox(checkbox_rect, v);
	}
	else
	{
		rect2_t label_rect, widget_rect;
		ui_row_split(builder, &label_rect, &widget_rect);

		ui_label(label_rect, label);

		rect2_t checkbox_rect;
		rect2_cut_from_left(widget_rect, ui_sz_aspect(1.0f), &checkbox_rect, NULL);

		checkbox_rect = rect2_cut_margins(checkbox_rect, ui_sz_pix(2.0f));

		return ui_checkbox(checkbox_rect, v);
	}
}

bool ui_row_slider_int(ui_row_builder_t *builder, string_t label, int *v, int min, int max)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);
	return ui_slider_int(widget_rect, v, min, max);
}

bool ui_row_slider(ui_row_builder_t *builder, string_t label, float *f, float min, float max)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);
	return ui_slider(widget_rect, f, min, max);
}

bool ui_row_slider_ex(ui_row_builder_t *builder, string_t label, float *f, float min, float max, float granularity)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);
	return ui_slider_ex(widget_rect, f, min, max, granularity);
}

void ui_row_text_edit_ex(ui_row_builder_t *builder, string_t label, dynamic_string_t *buffer, const ui_text_edit_params_t *params)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);
	ui_text_edit_ex(widget_rect, buffer, params);
}

void ui_row_text_edit(ui_row_builder_t *builder, string_t label, dynamic_string_t *buffer)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);
	ui_text_edit(widget_rect, buffer);
}

void ui_row_color_picker(ui_row_builder_t *builder, string_t label, v4_t *color)
{
	// TODO: Add alpha control

	ui_id_t id = ui_id_pointer(color);
	ui_validate_widget(id);

	if (!color)
	{
		ui_row_error_widget(builder, S("ui_row_color_picker"), S("Did not pass a color to ui_row_color_picker!"));
		return;
	}

	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label); 

	rect2_t color_block_rect = widget_rect;

	ui_draw_rect        (color_block_rect, *color);
	ui_draw_rect_outline(color_block_rect, make_v4(1, 1, 1, 0.1f), 1.0f);

	v3_t hsv = hsv_from_rgb(color->xyz).hsv;

	// string_t hsv_text = Sf("HSV: (%.02f, %.02f, %.02f)", hsv.x, hsv.y, hsv.z);
	string_t rgb_text = Sf("(%.02f, %.02f, %.02f, %.02f)", color->x, color->y, color->z, color->w);

	v4_t text_color = ui_color(UiColor_text_low);

	if (hsv.z > 0.5f)
	{
		text_color.xyz = mul(text_color.xyz, 0.05f);
	}

	UI_Color(UiColor_text, text_color)
	ui_draw_text_aligned(ui_font(UiFont_default), color_block_rect, rgb_text, make_v2(0.5f, 0.5f));

	bool first_use;
	ui_color_picker_state_t *state = ui_get_state(id, &first_use, ui_color_picker_state_t);

	if (ui_mouse_in_rect(color_block_rect))
	{
		ui_set_next_hot(id);
	}

	bool popup_opened_this_frame = false;

	if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			popup_opened_this_frame = true;

			state->popup_open           = true;
			state->popup_position       = ui->input.mouse_p;
			state->color_before_editing = *color;
		}
	}

	float popup_openness = ui_interpolate_f32(ui_child_id(id, S("openness")), state->popup_open);

	if (popup_opened_this_frame || popup_openness > 0.001f)
	{
		ui_push_sub_layer();

		if (state->popup_open)
		{
			if (ui_key_pressed(Key_escape, true))
			{
				state->popup_open = false;
				*color = state->color_before_editing;
			}
			else if (ui_key_pressed(Key_return, true))
			{
				state->popup_open = false;
			}
		}

		if (popup_opened_this_frame ||
			color->x != state->cached_color.x ||
			color->y != state->cached_color.y ||
			color->z != state->cached_color.z ||
			color->w != state->cached_color.w)
		{
			v3_t hsv = hsv_from_rgb(color->xyz).hsv;
			state->hue = hsv.x;
			state->sat = hsv.y;
			state->val = hsv.z;
		}

		v2_t    popup_size     = make_v2(popup_openness*318, popup_openness*256);
		v2_t    popup_position = add(state->popup_position, make_v2(0, -popup_size.y));
		rect2_t popup_rect     = rect2_from_min_dim(popup_position, popup_size);

		ui_push_clip_rect(ui->ui_area, false);

		ui_draw_rect_shadow (popup_rect, ui_color(UiColor_window_background), 0.5f, 16.0f);
		ui_draw_rect_outline(popup_rect, ui_color(UiColor_window_outline), 2.0f);

		popup_rect = rect2_cut_margins(popup_rect, ui_sz_pix(ui_scalar(UiScalar_outer_window_margin)));

		rect2_t sat_val_picker_rect;
		rect2_cut_from_left(popup_rect, ui_sz_aspect(1.0f), &sat_val_picker_rect, &popup_rect);
		rect2_cut_from_left(popup_rect, ui_sz_pix(ui_scalar(UiScalar_widget_margin)), NULL, &popup_rect);

		rect2_t hue_picker_rect;
		rect2_cut_from_left(popup_rect, ui_sz_aspect(0.1f), &hue_picker_rect, &popup_rect);
		rect2_cut_from_left(popup_rect, ui_sz_pix(ui_scalar(UiScalar_widget_margin)), NULL, &popup_rect);

		ui_label(label_rect, label);

		ui_hue_picker    (hue_picker_rect,     &state->hue);
		ui_sat_val_picker(sat_val_picker_rect,  state->hue, &state->sat, &state->val);

		v3_t rgb = rgb_from_hsv((v3_t){state->hue, state->sat, state->val});

		float font_height = ui_font(UiFont_default)->height;

		rect2_t hsv_rect, rgb_rect;
		rect2_cut_from_top(popup_rect, ui_sz_pix(font_height), &hsv_rect, &popup_rect);
		rect2_cut_from_top(popup_rect, ui_sz_pix(font_height), &rgb_rect, &popup_rect);

		ui_label(hsv_rect, Sf("HSV: (%.02f, %.02f, %.02f)", state->hue, state->sat, state->val));
		ui_label(rgb_rect, Sf("RGB: (%.02f, %.02f, %.02f)", rgb.x, rgb.y, rgb.z));

		color->xyz = rgb;

		ui_pop_clip_rect();

		ui_pop_sub_layer();
	}

	state->cached_color = *color;
}

void ui_row_color_picker_v3(ui_row_builder_t *builder, string_t label, v3_t *color)
{
	if (!color)
	{
		ui_row_error_widget(builder, S("ui_row_color_picker"), S("Did not pass a color to ui_row_color_picker_v3!"));
		return;
	}

    ui_set_next_id(ui_id_pointer(color));

    v4_t color_ = { color->x, color->y, color->z, 1.0f };
    ui_row_color_picker(builder, label, &color_);

    color->x = color_.x;
    color->y = color_.y;
    color->z = color_.z;
}
