void update_and_draw_console(console_t *console, v2_t resolution, float dt)
{
	(void)dt;

	float line_spacing = ui_default_row_height();

	float console_height = 17.0f*line_spacing;

	rect2_t console_area = rect2_from_min_dim(make_v2(0, resolution.y - console_height - 1.0f),
											  make_v2(resolution.x, console_height));

	ui->render_layer = UI_LAYER_BACKGROUND;

	v4_t console_background_color = ui_color(UI_COLOR_WINDOW_BACKGROUND);
	console_background_color.w = 0.5f;

	ui_draw_rect_shadow(console_area, console_background_color, 0.25f, 16.0f);

	ui_layout_t layout = ui_make_layout(console_area);
	ui_equip_layout(&layout);

	UI_Font(UiFont_default, console->font)
	{
		Layout_Flow(Flow_east)
		Layout_Cut(
			.size           = ui_sz_pix(line_spacing*15.0f),
			.flow           = Flow_south,
			.push_clip_rect = true)
		{
			for (table_iter_t iter = cvar_iter(); table_iter_next(&iter);)
			{
				cvar_t *cvar = iter.ptr;

				Layout_Cut(
					.size           = ui_sz_pix(line_spacing),
					.flow           = Flow_justify,
					.justify_x      = 0.0f,
					.justify_y      = 0.5f,
					.push_clip_rect = true)
				{
					if (!(iter.index & 1))
					{
						ui_draw_rect(layout_rect(), make_v4(0, 0, 0, 0.25f));
					}

					ui_draw_text_label_alignment(ui_font(UiFont_default), layout_rect(), cvar->key);
					// layout_label(cvar->key);
				}
			}
		}
		Layout_Flow(Flow_north)
		Layout_Cut(
			.size = ui_sz_pct(1.0f))
		{
			Layout_Cut(
				.size = ui_sz_pix(line_spacing))
			{
				// bool submit = false;

				// Layout_Flow(Flow_west)
				// submit = layout_button(S("Submit"));

				/*
				ui_text_edit_result_t result = ui_text_edit_ex(ui_id(S("console_input_box")), 
															   layout_rect(), 
															   &console->input, 
															   console->completion_count, 
															   console->completions);

				if ((result & UiTextEditResult_committed) || submit)
				{
					string_t text = console->input.string;
					console_handle_command(console, text);

					dyn_string_clear(&console->input);
				}
				else if (result & UiTextEditResult_edited)
				{
					string_t text = console->input.string;

					for (table_iter_t iter = cvar_iter(); table_iter_next(&iter);)
					{
						cvar_t *cvar = iter.ptr;


					}
				}
				*/
			}
			Layout_Cut(
				.size = ui_sz_pct(1.0f))
			{
				/*
				log_filter_t filter = console->log_filter;

				uint32_t line_count = log_get_filtered_line_count(filter);

				ui_id_t id = ui_id(S("console_log_virtual_list"));
				ui_list_region_t region = ui_virtual_list(id, layout_rect(), line_spacing, line_count);

				uint32_t line_index = 0;

				for (log_iter_t it = log_iter_filtered(filter, region.start);
					 log_iter_valid(&it);
					 log_iter_next(&it))
				{
					log_entry_t *entry = it.entry;

					if (!(layout_index & 1))
					{
						ui_draw_rect(layout_rect(), make_v4(0, 0, 0, 0.25f));
					}

					layout_label(entry->text);

					if (line_index >= region.end)
					{
						break;
					}

					line_index += 1;
				}
				*/
			}
		}
	}

	ui_unequip_layout();
}
