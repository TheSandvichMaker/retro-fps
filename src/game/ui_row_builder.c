ui_row_builder_t ui_make_row_builder(rect2_t rect)
{
	ui_row_builder_t result = {
		.rect = rect,
	};

	return result;
}

rect2_t ui_row_ex(ui_row_builder_t *builder, float height, bool draw_background)
{
	float margin = ui_scalar(UI_SCALAR_ROW_MARGIN);
	float bot_margin = ceilf(0.5f*margin);
	float top_margin = margin - bot_margin;

	rect2_t row;
	rect2_cut_from_top(builder->rect, ui_sz_pix(height + margin), &row, &builder->rect);

	if (draw_background)
	{
		UI_SCALAR(UI_SCALAR_ROUNDEDNESS, 0.0f)
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
	// && (builder->row_index & 1) == 0
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
	v2_t dim = ui_compute_header_size(label);

	rect2_t row = ui_row_ex(builder, dim.y, true);
	ui_header_new(row, label);
}

void ui_row_label(ui_row_builder_t *builder, string_t label)
{
	rect2_t rect = ui_row(builder);
	ui_label_new(rect, label);
}

fn void ui_row_progress_bar(ui_row_builder_t *builder, string_t label, float progress)
{
	float height = 0.75f*ui_default_row_height();

	rect2_t rect = ui_row_ex(builder, height, false);
	ui_progress_bar_new(rect, label, progress);
}

bool ui_row_button(ui_row_builder_t *builder, string_t label)
{
	rect2_t rect = ui_row(builder);
	return ui_button_new(rect, label);
}

bool ui_row_radio_buttons(ui_row_builder_t *builder, string_t label, int *state, string_t *option_labels, int option_count)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label_new(label_rect, label);

	int current_state = state ? *state : 0;
	int new_state = current_state;

	if (option_count > 0)
	{
		float margin       = ui_scalar(UI_SCALAR_WIDGET_MARGIN);
		float margin_space = (float)(option_count - 1)*margin;
		float button_width = (rect2_width(widget_rect) - margin_space) / (float)option_count;

		for (int i = 0; i < option_count; i++)
		{
			rect2_t button_rect;
			rect2_cut_from_left(widget_rect, ui_sz_pix(button_width), &button_rect, &widget_rect);
			rect2_cut_from_left(widget_rect, ui_sz_pix(margin), NULL, &widget_rect);

			bool active = (current_state == i);

			UI_ColorConditional(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_BUTTON_ACTIVE), active)
			if (ui_button_new(button_rect, option_labels[i]))
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
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label_new(label_rect, label);

	rect2_t checkbox_rect;
	rect2_cut_from_right(widget_rect, ui_sz_aspect(1.0f), &checkbox_rect, NULL);

	return ui_checkbox_new(checkbox_rect, v);
}

bool ui_row_slider_int(ui_row_builder_t *builder, string_t label, int *v, int min, int max)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label_new(label_rect, label);
	return ui_slider_int_new(widget_rect, v, min, max);
}

fn bool ui_row_slider(ui_row_builder_t *builder, string_t label, float *f, float min, float max)
{
	rect2_t label_rect, widget_rect;
	ui_row_split(builder, &label_rect, &widget_rect);

	ui_label_new(label_rect, label);
	return ui_slider_new(widget_rect, f, min, max);
}
