//
// unified interface:
//

void ui_widget_checkbox(ui_widget_context_t *context, ui_widget_mode_t mode, rect2_t rect)
{
	ui_id_t id = context->id;

	switch (mode)
	{
		case UIWidgetMode_interact:
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
			rect2_hsplit(rect, height, &checkbox_rect, &label_rect);

			v4_t color = ui_button_colors(id);

			float roundedness_ratio = min(0.66f, ui_roundedness_ratio(checkbox_rect));
			float outer_roundedness = ui_roundedness_ratio_to_abs(checkbox_rect, roundedness_ratio);
			float inner_roundedness = ui_roundedness_ratio_to_abs(check_rect, roundedness_ratio);

			UI_Scalar(UiScalar_roundedness, outer_roundedness)
			{
				rect2_t checkbox_shadow = checkbox_rect;
				checkbox_rect = rect2_add_offset(checkbox_rect, make_v2(0, hover_lift));

				ui_draw_rect_outline(checkbox_shadow, ui_color(UiColor_widget_shadow), checkbox_thickness_pixels);
				ui_draw_rect_outline(checkbox_rect, color, checkbox_thickness_pixels);
			}

			if (context->is_active) 
			{
				UI_Scalar(UiScalar_roundedness, inner_roundedness)
				{
					rect2_t check_shadow = check_rect;
					check_rect = rect2_add_offset(check_rect, make_v2(0, hover_lift));

					ui_draw_rect(check_shadow, ui_color(UiColor_widget_shadow));
					ui_draw_rect(check_rect, color);
				}
			}

			v2_t p          = ui_text_align_p(ui->style.font, label_rect, label, make_v2(0.0f, 0.5f));
			v4_t text_color = ui_interpolate_v4(ui_child_id(id, S("text_color")), ui_color(UiColor_text));

			UI_Color(UiColor_text, text_color)
			{
				ui_draw_text(ui->style.font, p, label);
			}
		} break;
	}
}

// immediate, manual layout
void ui_checkbox(string_t label, rect2_t rect, bool *value)
{
	ui_widget_context_t context = {
		.userdata = value,
	};

	ui_widget_checkbox(&context, UIWidgetMode_interact_and_draw, rect);
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

	UI_Scalar(UiScalar_roundedness, outer_roundedness)
	{
		rect2_t checkbox_shadow = checkbox_rect;
		checkbox_rect = rect2_add_offset(checkbox_rect, make_v2(0, hover_lift));

		ui_draw_rect_outline(checkbox_shadow, ui_color(UiColor_widget_shadow), checkbox_thickness_pixels);
		ui_draw_rect_outline(checkbox_rect, color, checkbox_thickness_pixels);
	}

	if (context->active) 
	{
		UI_Scalar(UiScalar_roundedness, inner_roundedness)
		{
			rect2_t check_shadow = check_rect;
			check_rect = rect2_add_offset(check_rect, make_v2(0, hover_lift));

			ui_draw_rect(check_shadow, ui_color(UiColor_widget_shadow));
			ui_draw_rect(check_rect, color);
		}
	}

	v2_t p          = ui_text_align_p(ui->style.font, label_rect, label, make_v2(0.0f, 0.5f));
	v4_t text_color = ui_interpolate_v4(ui_child_id(id, S("text_color")), ui_color(UiColor_text));

	UI_Color(UiColor_text, text_color)
	{
		ui_draw_text(ui->style.font, p, label);
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
		ui_widget_checkbox(&context, UIWidgetMode_interact, rect);
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

// haha I don't know what I want
// let's sketch out Rect Cut But Better

void ui_checkbox(string_t label, rect2_t rect, bool *value);

void layout_test(rect2_t panel)
{
	ui_layout_t layout = make_layout(panel);

	equip_layout(&layout);

	// the idea I guess is that ui_xxx widgets take an explicit rect
	// and layout_xxx widgets cut out a rect automatically based on
	// the layout parameters and such

	// first, I want a title bar
	Layout_Flow(Flow_south)
	{
		Layout_Cut(
			.size = ui_sz_pix(16.0f))
		{
			// there's three buttons on the left that want to occupy 1/3rd of the space evenly
			Layout_Flow(Flow_east)
			{
				Layout_Cut(
					.size = ui_sz_pct(1.0f / 3.0f))
				{
					// prepare even spacing for 3 rects, this overrides the rects used normally
					// by the buttons
					layout_prepare_even_spacing(3);

					if (layout_button(S("File")))
					{
						do_file_menu();
					}

					if (layout_button(S("Edit")))
					{
						do_edit_menu();
					}

					if (layout_button(S("View")))
					{
						do_view_menu();
					}
				}
			}

			// there's three buttons on the right of (possibly) varying size
			Layout_Flow(Flow_west)
			{
				if (layout_icon_button(UiIcon_close))
				{
					editor_command(EditorCommand_close, NULL);
				}

				if (is_windowed)
				{
					if (layout_icon_button(UiIcon_fullscreen))
					{
						editor_command(EditorCommand_fullscreen, NULL);
					}
				}
				else
				{
					if (layout_icon_button(UiIcon_windowed))
					{
						editor_command(EditorCommand_windowed, NULL);
					}
				}

				if (layout_icon_button(UiIcon_minimize))
				{
					editor_command(EditorCommand_minimize, NULL);
				}
			}

			// now I want to center some text in the remaining space... somehow
			// not sure how to make that not annoying to do, the thing doesn't
			// want to cut anything anymore, it just wants to use the remaining 
			// space, so maybe that remaining space is accessible through this
			// cool variable 'rect'

			ui_label(rect, S("Centered Text"));

			// but how do you even decide that ui_label would center the label
			// if given an oversized rect, that seems like now the simple widget
			// code that is supposed to just accept rects is doing layout again

			// so maybe it should look like this
			Layout_Justify(
				.x = 0.5, // 0.5 = center
				.y = 0.5) // 0.5 = center
			{
				// and then setting that up makes sense with this somehow
				layout_label(S("Centered Text"));
				// but layout_label now is not doing a simple rect cut, it's
				// conceptually doing 4 rect cuts to carve out the empty space
				// that centers the label (maybe in the implementation it doesn't
				// really need to do that)
			}
		}
	}

	unequip_layout(layout);
}

void layout_place_widget(ui_widget_context_t *context, ui_widget_func_t widget)
{
	rect2_t rect = {0};

	widget(context, UIWidgetMode_get_size, rect);

	v2_t bounds = context->min_size;

	if (layout->wants_justify)
	{
		// justify stuff
		rect = layout_find_justified_rect(bounds);
	}
	else
	{
		// cut stuff, maybe insert margins here somehow
		rect = layout_cut(layout->flow_direction, ui_sz_pix(bounds.e[layout->flow_axis]));
	}

	// maybe interact and draw doesn't need to be separate in this scheme
	widget(context, UIWidgetMode_interact_and_draw, rect);
}

