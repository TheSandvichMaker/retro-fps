void update_and_draw_console(console_t *console, v2_t resolution, float dt)
{
	(void)dt;
	(void)console;
	(void)resolution;

#if 0
	float line_spacing = ui_default_row_height();

	float console_height = 17.0f*line_spacing;

	rect2_t console_area = rect2_from_min_dim(make_v2(0, resolution.y - console_height - 1.0f),
											  make_v2(resolution.x, console_height));

	ui->render_layer = UI_LAYER_BACKGROUND;

	v4_t console_background_color = ui_color(UiColor_window_background);
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
#endif
}

void editor_init_cvar_window(cvar_window_state_t *state)
{
	state->search_string = dynamic_string_on_storage(state->search_string_storage);
}

int cvar_compare_alphabetic(const void *a, const void *b, void *user_data)
{
	(void)user_data;

	const cvar_t *cvar_a = *(cvar_t **)a;
	const cvar_t *cvar_b = *(cvar_t **)b;

	return string_compare(cvar_a->key, cvar_b->key, StringMatch_case_insensitive);
}

int cvar_compare_edit_distance(const void *a, const void *b, void *user_data)
{
	const string_t *search_string = user_data;

	const cvar_t *cvar_a = *(cvar_t **)a;
	const cvar_t *cvar_b = *(cvar_t **)b;

	// this is bad and slow
	int edit_distance_a = calculate_edit_distance(*search_string, cvar_a->key, StringMatch_case_insensitive);
	int edit_distance_b = calculate_edit_distance(*search_string, cvar_b->key, StringMatch_case_insensitive);

	return edit_distance_a > edit_distance_b;
}

void editor_do_cvar_window(cvar_window_state_t *state, editor_window_t *window)
{
	rect2_t window_rect = rect2_cut_margins(window->window.rect, ui_sz_pix(ui_scalar(UiScalar_outer_window_margin)));

	//------------------------------------------------------------------------
	// Exclude search bar from the scroll region

	ui_row_builder_t outer_builder = ui_make_row_builder(window_rect);

	rect2_t search_row = ui_row(&outer_builder);

	ui_text_edit_result_t edit_result = ui_text_edit_ex(search_row, &state->search_string, &(ui_text_edit_params_t){
		.preview_text = S("Search..."),
	});

	if (edit_result & UiTextEditResult_edited)
	{
		window->scroll_region.scroll_offset.y = 0.0f;
	}

	if (edit_result & UiTextEditResult_terminated)
	{
		dyn_string_clear(&state->search_string);
	}

	string_t search_string = state->search_string.string;

	ui_row_spacer(&outer_builder, ui_sz_pix(ui_scalar(UiScalar_outer_window_margin)));

	//------------------------------------------------------------------------
	// Console variables area

	rect2_t cvar_area = outer_builder.rect;

	v4_t region_color_outline = ui_color(UiColor_region_outline);

	ui_draw_rect_outline(cvar_area, region_color_outline, 1.0f);
	cvar_area = rect2_cut_margins(cvar_area, ui_sz_pix(ui_scalar(UiScalar_outer_window_margin) + 1.0f));

	ui_row_builder_t builder = ui_make_row_builder_ex(
		ui_scrollable_region_begin(&window->scroll_region, cvar_area), 
		&(ui_row_builder_params_t){
			.flags = UiRowBuilder_insert_row_separators,
		});

	arena_t *temp = m_get_temp_scope_begin(NULL, 0);

	size_t cvar_count = get_cvar_count();
	cvar_t **cvars = m_alloc_array_nozero(temp, cvar_count, cvar_t *);

	for (table_iter_t iter = cvar_iter(); table_iter_next(&iter);)
	{
		cvars[iter.i] = iter.ptr;
	}

	if (search_string.count > 0)
	{
		merge_sort_array(cvars, cvar_count, cvar_compare_edit_distance, &search_string);
	}
	else
	{
		merge_sort_array(cvars, cvar_count, cvar_compare_alphabetic, NULL);
	}

	//------------------------------------------------------------------------

	ui_push_color(UiColor_text, ui_color(UiColor_text_low));

	for (size_t i = 0; i < cvar_count; i++)
	{
		cvar_t *cvar = cvars[i];

		if (search_string.count > 0)
		{
			if (find_substring(cvar->key, search_string, StringMatch_case_insensitive) == STRING_NPOS)
			{
				// no match found in the search string - skip.
				continue;
			}
		}

		rect2_t row = ui_row(&builder);

		rect2_t reset_rect;
		rect2_cut_from_right(row, ui_sz_pix(18.0f), &reset_rect, &row);
		rect2_cut_from_right(row, ui_sz_pix(ui_scalar(UiScalar_widget_margin)), NULL, &row);

		float widget_size = flt_max(64.0f, ui_size_to_width(row, ui_sz_pct(0.3333f)));

		rect2_t label_rect, widget_rect;
		rect2_cut_from_right(row, ui_sz_pix(widget_size), &widget_rect, &label_rect);

		ui_label(label_rect, cvar->key);

		switch (cvar->kind)
		{
			case CVarKind_bool:
			{
				rect2_t checkbox_rect;
				rect2_cut_from_left(widget_rect, ui_sz_aspect(1.0f), &checkbox_rect, NULL);

				checkbox_rect = rect2_cut_margins(checkbox_rect, ui_sz_pix(2.0f));

				ui_checkbox(checkbox_rect, &cvar->as.boolean);
			} break;

			case CVarKind_i32:
			{
				ui_slider_int(widget_rect, &cvar->as.i32, cvar->min.i32, cvar->max.i32);
			} break;

			case CVarKind_f32:
			{
				ui_slider(widget_rect, &cvar->as.f32, cvar->min.f32, cvar->max.f32);
			} break;

			case CVarKind_string:
			{
				ui_label(widget_rect, Sf("\"%cs\"", cvar->as.string));
			} break;

			case CVarKind_command:
			{
				if (ui_button(widget_rect, S("Run")))
				{
					cvar_execute_command(cvar, S(""));
				}
			} break;

			default:
			{
				ui_label(widget_rect, Sf("%cs (unhandled cvar type)", cvar->key));
			} break;
		}

		if (!cvar_is_default(cvar))
		{
			ui_push_id(ui_id(cvar->key));

			if (ui_button(reset_rect, S("X")))
			{
				cvar_reset_to_default(cvar);
			}

			ui_pop_id();
		}
	}

	ui_pop_color(UiColor_text);

	//------------------------------------------------------------------------

	ui_scrollable_region_end(&window->scroll_region, builder.rect);

	m_scope_end(temp);
}