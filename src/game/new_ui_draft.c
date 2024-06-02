//
// unified interface:
//

void ui_widget_checkbox(ui_widget_context_t *context, ui_widget_mode_t mode, rect2_t rect)
{
	ui_id_t id = context->id;

	switch (mode)
	{
		case UIWidgetMode_input:
		{
			if (ui_is_active(id))
			{
				if (ui_button_released(Button_left))
				{
					*context->bool = *context->bool;
				}
			}
			else if (ui_is_hot(id))
			{
				if (ui_button_pressed_in_rect(Button_left, rect))
				{
					ui_set_active(id);
				}
			}
			else if (ui_mouse_in_rect(rect))
			{
				ui_set_next_hot(id);
			}
		} break;

		case UIWidgetMode_get_size:
		{
			float label_width   = ui_text_width(ui_font(UIFont_label), label);
			float label_padding = ui_scalar(UIScalar_text_margin);

			v2_t min_widget_size;
			min_widget_size.y = ui_font_height();
			min_widget_size.x = label_width + label_padding + min_widget_size.y;

			context->min_size = min_widget_size;
		} break;

		case UIWidgetMode_draw:
		{
			float height = rect2_height(rect);

			rect2_t checkbox_rect, label_rect;
			rect_hsplit(rect, height, &checkbox_rect, &label_rect);

			v4_t color = ui_button_colors(id);

			float roundedness_ratio = min(0.66f, ui_roundedness_ratio(checkbox_rect));
			float outer_roundedness = ui_roundedness_ratio_to_abs(checkbox_rect, roundedness_ratio);
			float inner_roundedness = ui_roundedness_ratio_to_abs(check_rect, roundedness_ratio);

			UI_SCALAR(UI_SCALAR_ROUNDEDNESS, outer_roundedness)
			{
				rect2_t checkbox_shadow = checkbox_rect;
				checkbox_rect = rect2_add_offset(checkbox_rect, make_v2(0, hover_lift));

				ui_draw_rect_outline(checkbox_shadow, ui_color(UI_COLOR_WIDGET_SHADOW), checkbox_thickness_pixels);
				ui_draw_rect_outline(checkbox_rect, color, checkbox_thickness_pixels);
			}

			if (context->is_active) 
			{
				UI_SCALAR(UI_SCALAR_ROUNDEDNESS, inner_roundedness)
				{
					rect2_t check_shadow = check_rect;
					check_rect = rect2_add_offset(check_rect, make_v2(0, hover_lift));

					ui_draw_rect(check_shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
					ui_draw_rect(check_rect, color);
				}
			}

			v2_t p          = ui_text_align_p(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));
			v4_t text_color = ui_interpolate_v4(ui_child_id(id, S("text_color")), ui_color(UI_COLOR_TEXT));

			UI_COLOR(UI_COLOR_TEXT, text_color)
			{
				ui_draw_text(&ui.style.font, p, label);
			}
		} break;
	}
}

// immediate, manual layout
void ui_checkbox(string_t label, rect2_t rect, bool *value)
{
	ui_widget_context_t context = {
		.bool = value,
	};

	if (value)
	{
		ui_widget_checkbox(&context, UIWidgetMode_input, rect);
	}

	ui_widget_checkbox(&context, UIWidgetMode_draw, rect);
}

// auto-layout with frame latency on hit testing and deferred draw (but no frame latency on draw)
void layout_checkbox(string_t label, bool *value)
{
	ui_id_t id = ui_child_id(ui_id_ptr(value), label);

	ui_widget_context_t context = {
		.id              = id,
		.checkbox_target = value,
		.widget          = ui_checkbox,
	};

	layout_execute_widget(&context);
}

// ok this actually is maybe a viable idea:

void layout_example(void)
{
	Layout_Container(
		.axis   = Axis_y,
		.margin = 4.0)
	{
		UI_Font(
			.font  = ui_font(UIFont_header),
			.color = ui_color(UIColor_header))
		{
			layout_checkbox(S("Checkbox #1"), &bool_1);
			layout_checkbox(S("Checkbox #2"), &bool_2);
			layout_checkbox(S("Checkbox #3"), &bool_3);
		}

		Layout_Container(
			.axis   = Axis_x,
			.margin = 4.0)
		{
			layout_checkbox(S("Checkbox #4"), &bool_4);
			layout_checkbox(S("Checkbox #5"), &bool_5);
		}
	}

	Layout_Container(
		.axis = Axis_y)
	{
		Layout_Box(
			.font        = ui_font(UIFont_header),
			.text        = S("Header"),
			.roundedness = { 0, 0, 4, 4 },
			.size        = ui_sz_text(1.0),
			.background  = ui_color(UIColor_header_background));


		Layout_Container(
			.axis = Axis_y)
		{
			for (size_t i = 0; i < item_count; i++)
			{
				item_t *item = &items[i];

				v4_t color = ui_color(i % 2 == 0 ? UIColor_list_item_background_1 : UIColor_list_item_background_2);

				Layout_Container(
					.axis = Axis_x,
					.size = ui_sz_children(1.0))
				{
					Layout_Box(
						.font = ui_font(UIFont_label),
						.text = item->label,
						.size = ui_sz_fill(1.0),
						.background = color);
						
					Layout_Box(
						.size = ui_sz_fill(1.0),
						.background = color)
					{
						UI_Margin(
							.x = ui_sz_fill(1.0),
							.y = ui_sz_fill(1.0))
						{
							layout_checkbox(&item->value);
						}
					}
			}
		}

		Layout_Box(
			.roundedness = { 4, 4, 0, 0 },
			.background  = ui_color(UIColor_list_footer),
			.size        = ui_sz_pix(16.0f));
	}
}

//
// separate interface:
//

void ui_widget_checkbox__get_rects(rect2_t rect, rect2_t *checkbox_rect, rect2_t *label_rect)
{
	float height = rect2_height(rect);

	rect2_t checkbox_rect, label_rect;
	rect_hsplit(rect, height, checkbox_rect, label_rect);
}

bool ui_widget_checkbox_interact(ui_id_t id, rect2_t rect, bool *value)
{
	// one benefit of this design is I can easily return whatever I want
	bool result = false;

	// let's pretend we _want_ the checkbox rect to respond to input only (I don't but it's for the sake of argument)
	// so this is now duplicate code:
	rect2_t checkbox_rect, label_rect;
	ui_widget_checkbox__get_rects(rect, &checkbox_rect, &label_rect);

	if (ui_is_active(id))
	{
		if (ui_button_released(Button_left))
		{
			*context->bool = !*context->bool;
			result = true;
		}
	}
	else if (ui_is_hot(id))
	{
		if (ui_button_pressed_in_rect(Button_left, rect))
		{
			ui_set_active(id);
		}
	}
	else if (ui_mouse_in_rect(rect))
	{
		ui_set_next_hot(id);
	}

	return result;
}

void ui_widget_checkbox_draw(ui_widget_context_t *context, rect2_t rect)
{
	ui_id_t id = context->id;

	// so this is now duplicate code:
	rect2_t checkbox_rect, label_rect;
	ui_widget_checkbox__get_rects(rect, &checkbox_rect, &label_rect);

	v4_t color = ui_button_colors(id);

	float roundedness_ratio = min(0.66f, ui_roundedness_ratio(checkbox_rect));
	float outer_roundedness = ui_roundedness_ratio_to_abs(checkbox_rect, roundedness_ratio);
	float inner_roundedness = ui_roundedness_ratio_to_abs(check_rect, roundedness_ratio);

	UI_SCALAR(UI_SCALAR_ROUNDEDNESS, outer_roundedness)
	{
		rect2_t checkbox_shadow = checkbox_rect;
		checkbox_rect = rect2_add_offset(checkbox_rect, make_v2(0, hover_lift));

		ui_draw_rect_outline(checkbox_shadow, ui_color(UI_COLOR_WIDGET_SHADOW), checkbox_thickness_pixels);
		ui_draw_rect_outline(checkbox_rect, color, checkbox_thickness_pixels);
	}

	if (context->active) 
	{
		UI_SCALAR(UI_SCALAR_ROUNDEDNESS, inner_roundedness)
		{
			rect2_t check_shadow = check_rect;
			check_rect = rect2_add_offset(check_rect, make_v2(0, hover_lift));

			ui_draw_rect(check_shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
			ui_draw_rect(check_rect, color);
		}
	}

	v2_t p          = ui_text_align_p(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));
	v4_t text_color = ui_interpolate_v4(ui_child_id(id, S("text_color")), ui_color(UI_COLOR_TEXT));

	UI_COLOR(UI_COLOR_TEXT, text_color)
	{
		ui_draw_text(&ui.style.font, p, label);
	}
}

// immediate, manual layout
bool ui_checkbox(string_t label, rect2_t rect, bool *value)
{
	bool result = false;

	ui_id_t id = ui_id_ptr(value);

	if (value)
	{
		result = ui_widget_checkbox_interact(id, rect, value);
	}

	ui_widget_context_t context = {
		.id     = id,
		.active = value ? *value : false,
	};

	ui_widget_checkbox_draw(&context, rect);
}

// auto-layout with frame latency on hit testing and deferred draw (but no frame latency on draw)
void layout_checkbox(layout_t *layout, string_t label, bool *value)
{
	ui_widget_context_t context = {
		.id   = ui_id_ptr(value),
		.active = value,
	};

	rect2_t rect = layout_push_widget(layout, context, ui_checkbox);

	if (value)
	{
		ui_widget_checkbox(&context, UIWidgetMode_input, rect);
	}
}

//
// UI command buffer
//

typedef enum ui_cmd_kind_t
{
	UICmd_push,
	UICmd_pop,
	UICmd_hbox,
	UICmd_vbox,
	UICmd_checkbox,
} ui_cmd_kind_t;

typedef struct ui_cmd_checkbox_t
{
	ui_cmd_header_t header;
	bool *value;
} ui_cmd_checkbox_t;

void do_layout(void)
{
	ui_rect_t *root   = ...;
	ui_rect_t *last   = root;
	ui_rect_t *parent = NULL;

	for (ui_cmd_header_t *header = start;
		 header < end;
		 header = ui_cmd_next(header))
	{
		switch (header->kind)
		{
			case UICmd_push:
			{
				parent = last;
			} break;

			case UICmd_hbox:
			{
				last = ui_rect(parent, 
			} break;

			case UICmd_vbox:
			{
			} break;
		}
	}
}

// ok the command buffer just seems worse than building the tree on the fly
// what if slate but C/immediate:

void layout_checkbox_label(string_t label, bool *target)
{
	Layout_Push(vbox(ui_fill(1.0)))
	{
		Layout_Push(hbox(ui_aspect(1.0)))
		{
			layout_checkbox(target);
		}
		Layout_Push(hbox(ui_fill(1.0)))
		{
			layout_label(label);
		}
	}
}

void example(void)
{
	ui_layout_t *layout = ...;

	equip_layout(layout);

	Layout_Push(hbox(ui_fill(1.0)));
	{
		Layout_Push(vbox(ui_fill(1.0))) layout_checkbox_label(S("Checkbox #1"), &my_bool1);
		Layout_Push(vbox(ui_fill(1.0))) layout_checkbox_label(S("Checkbox #2"), &my_bool2);
		Layout_Push(vbox(ui_fill(1.0))) layout_checkbox_label(S("Checkbox #3"), &my_bool3);
		Layout_Push(vbox(ui_fill(1.0))) layout_checkbox_label(S("Checkbox #4"), &my_bool4);

		Layout_Push(vbox(ui_fill(1.0))) 
		{
			Layout_Push(hbox(ui_fill(1.0))) layout_checkbox(&my_bool5);
			Layout_Push(hbox(ui_fill(1.0))) layout_checkbox(&my_bool6);
		}
	}
}

// well, you get the same awful verbosity of slate, basically...

// ok this sucks, and I don't want it. I think what I actually want is:

void ui_checkbox(string_t label, rect2_t rect, bool *value);

void layout_checkbox(string_t label, bool *value)
{

}
