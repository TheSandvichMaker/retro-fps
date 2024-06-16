ui_row_builder_t ui_make_row_builder(rect2_t rect)
{
	ui_row_builder_t result = {
		.rect = rect,
	};

	return result;
}

rect2_t ui_row_ex(ui_row_builder_t *builder, float height, bool draw_background)
{
	float margin = ui_scalar(UiScalar_row_margin);
	float bot_margin = ceilf(0.5f*margin);
	float top_margin = margin - bot_margin;

	rect2_t row;
	rect2_cut_from_top(builder->rect, ui_sz_pix(height + margin), &row, &builder->rect);

	if (draw_background)
	{
		UI_Scalar(UiScalar_roundedness, 0.0f)
		ui_draw_rect(row, make_v4(0, 0, 0, 0.25f));
	}

	builder->row_index += 1;

	rect2_t result = row;
	rect2_cut_from_bottom(result, ui_sz_pix(bot_margin), NULL, &result);
	rect2_cut_from_top   (result, ui_sz_pix(top_margin), NULL, &result);

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

			UI_Scalar          (UiScalar_roundedness, 0.0f)
			UI_Color           (UiColor_roundedness, roundedness)
			UI_ColorConditional(UiColor_button_idle, ui_color(UiColor_button_active), active)
			if (ui_button(button_rect, option_labels[i]))
			{
				new_state = i;
			}
		}
	}

	if (state)
	{
		*state = new_state;
	}

	bool result = current_state != new_state;
	return result;
}

bool ui_row_checkbox(ui_row_builder_t *builder, string_t label, bool *v)
{
#if UI_ROW_CHECKBOX_ON_LEFT
	rect2_t row = ui_row(builder);

	rect2_t checkbox_rect;
	rect2_cut_from_left(row, ui_sz_aspect(1.0f),                           &checkbox_rect, &row);
	rect2_cut_from_left(row, ui_sz_pix(ui_scalar(UiScalar_widget_margin)), NULL,           &row);

	ui_label(row, label);
	return ui_checkbox(checkbox_rect, v);
#else
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);

	rect2_t checkbox_rect;
	rect2_cut_from_right(widget_rect, ui_sz_aspect(1.0f), &checkbox_rect, NULL);

	return ui_checkbox(checkbox_rect, v);
#endif
}

bool ui_row_slider_int(ui_row_builder_t *builder, string_t label, int *v, int min, int max)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);
	return ui_slider_int(widget_rect, v, min, max);
}

bool ui_row_slider_int_ex(ui_row_builder_t *builder, string_t label, int *v, int min, int max, ui_slider_flags_t flags)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label(label_rect, label);
	return ui_slider_int_ex(widget_rect, v, min, max, flags);
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
	return ui_slider_ex(widget_rect, f, min, max, granularity, 0);
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

	float height = 6.0f*ui_font(UiFont_header)->height;
	rect2_t row = ui_row_ex(builder, height, true);

	if (!color)
	{
		ui_error_widget(row, S("ui_row_color_picker"), S("no color passed"));
		return;
	}

	ui_id_t id = ui_id(label);
	ui_validate_widget(id);

	bool first_use;
	ui_color_picker_state_t *state = ui_get_state(id, &first_use, ui_color_picker_state_t);

	if (first_use ||
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

	rect2_t label_rect, widget_rect;
	rect2_cut_from_left(row, ui_sz_pct(0.5f), &label_rect, &widget_rect);

	rect2_t sat_val_picker_rect;
	rect2_cut_from_left(widget_rect, ui_sz_aspect(1.0f), &sat_val_picker_rect, &widget_rect);
	rect2_cut_from_left(widget_rect, ui_sz_pix(ui_scalar(UiScalar_widget_margin)), NULL, &widget_rect);

	rect2_t hue_picker_rect;
	rect2_cut_from_left(widget_rect, ui_sz_aspect(0.2f), &hue_picker_rect, &widget_rect);
	rect2_cut_from_left(widget_rect, ui_sz_pix(ui_scalar(UiScalar_widget_margin)), NULL, &widget_rect);

	ui_label(label_rect, label);

	ui_hue_picker    (hue_picker_rect,     &state->hue);
	ui_sat_val_picker(sat_val_picker_rect,  state->hue, &state->sat, &state->val);

	v3_t rgb = rgb_from_hsv((v3_t){state->hue, state->sat, state->val});

	float font_height = ui_font(UiFont_default)->height;

	rect2_t hsv_rect, rgb_rect;
	rect2_cut_from_top(widget_rect, ui_sz_pix(font_height), &hsv_rect, &widget_rect);
	rect2_cut_from_top(widget_rect, ui_sz_pix(font_height), &rgb_rect, &widget_rect);

	ui_label(hsv_rect, Sf("HSV: (%.02f, %.02f, %.02f)", state->hue, state->sat, state->val));
	ui_label(rgb_rect, Sf("RGB: (%.02f, %.02f, %.02f)", rgb.x, rgb.y, rgb.z));

	color->xyz = rgb;
	state->cached_color = *color;
}
