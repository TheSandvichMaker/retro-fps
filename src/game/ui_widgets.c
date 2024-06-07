//
// Button
//

v2_t ui_compute_button_size(string_t label)
{
	v2_t size = {
		.x = ui_text_width(ui_font(UiFont_default), label),
		.y = ui_font(UiFont_default)->height,
	};

	return size;
}

void ui_button_new(rect2_t rect, string_t label)
{
	ui_id_t id = ui_id(label);

	bool result = false;

	// @UiHoverable
	ui_hoverable(id, rect);

	ui_style_color_t color_id = UI_COLOR_BUTTON_IDLE;

	if (ui_is_active(id))
	{
		if (ui_button_released(UiButton_left, true))
		{
			float   release_margin = 8.0f;
			rect2_t release_rect   = rect2_add_radius(rect, release_margin);

			if (ui_mouse_in_rect(release_rect))
			{
				result = true;
			}
		}

		color = UI_COLOR_BUTTON_ACTIVE;
	}
	else if (ui_is_hot(id))
	{
		if (ui_button_pressed(UiButton_left, true))
		{
			ui_set_active(id);
		}

		color = UI_COLOR_BUTTON_HOT;
	}
	else if (ui_mouse_in_rect(rect))
	{
		// @UiPriority
		ui_set_next_hot(id, UI_PRIORITY_DEFAULT);
	}

	v4_t color        = ui_color(color_id);
	v4_t color_interp = ui_interpolate_v4(id, color);

	ui_draw_rect        (rect, color_interp);
	ui_draw_text_aligned(ui_font(UiFont_default), rect, label, make_v2(0.5f, 0.5f));

	return result;
}

//
// Checkbox
//

v2_t ui_compute_checkbox_size(string_t label)
{
	float font_height = ui_font(UiFont_default)->height;

	v2_t size = {
		.x = font_height,
		.y = font_height,
	};

	return size;
}

bool ui_checkbox_new(rect2_t rect, bool *result_value);
