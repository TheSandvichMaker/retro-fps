void editor_do_ui_test_window(editor_ui_test_state_t *state, editor_window_t *window)
{
	ui_scrollable_region_flags_t scroll_flags = 
		UiScrollableRegionFlags_scroll_vertical|
		UiScrollableRegionFlags_draw_scroll_bar;

	rect2_t rect = window->rect;
	rect = rect2_shrink(rect, 1.0f);

	ui_id_t scroll_region_id = ui_child_id(ui_id_pointer(state), S("scroll_region"));
	rect2_t content_rect = ui_scrollable_region_begin_ex(scroll_region_id, rect, scroll_flags);

	content_rect = rect2_shrink(content_rect, 1.0f);

	ui_row_builder_t builder = ui_make_row_builder(content_rect);

	if (window->hovered)
	{
		ui_tooltip(S("Window for testing out various UI features."));
	}

	local_persist bool check_me = false;
	ui_hover_tooltip(S("This checkbox does nothing! GOOD DAY SIR!!"));
	ui_row_checkbox(&builder, S("Checkbox That Does Nothing"), &check_me);

	ui_row_slider    (&builder, S("Float Slider"), &state->slider_f32, -1.0f, 1.0f);
	ui_row_slider_int(&builder, S("Int Slider"), &state->slider_i32, 0, 8);

	local_persist int font_size = 18;
	if (ui_row_slider_int(&builder, S("UI Font Size"), &font_size, 8, 32))
	{
		ui_set_font_height((float)font_size);
	}

	ui_hover_tooltip(S("Size of the inner margin for windows"));
	ui_row_slider_ex(&builder, S("UI Window Margin"), &ui->style.base_scalars[UiScalar_window_margin], 0.0f, 8.0f, 1.0f);

	ui_hover_tooltip(S("Size of the margin between widgets"));
	ui_row_slider_ex(&builder, S("UI Widget Margin"), &ui->style.base_scalars[UiScalar_widget_margin], 0.0f, 8.0f, 1.0f);

	ui_hover_tooltip(S("Size of the margin between widgets and their text contents (e.g. a button and its label)"));
	ui_row_slider_ex(&builder, S("UI Text Margin"), &ui->style.base_scalars[UiScalar_text_margin], 0.0f, 8.0f, 1.0f);

	ui_row_slider_ex(&builder, S("UI Row Margin"), &ui->style.base_scalars[UiScalar_row_margin], 0.0f, 8.0f, 1.0f);

	ui_hover_tooltip(S("Roundedness of UI elements in pixel radius"));
	ui_row_slider_ex(&builder, S("UI Roundedness"), &ui->style.base_scalars[UiScalar_roundedness], 0.0f, 12.0f, 1.0f);

	ui_hover_tooltip(S("Spring stiffness coefficient for animations"));
	ui_row_slider(&builder, S("UI Animation Stiffness"), &ui->style.base_scalars[UiScalar_animation_stiffness], 1.0f, 1024.0f);

	ui_hover_tooltip(S("Spring dampen coefficient for animations"));
	ui_row_slider(&builder, S("UI Animation Dampen"), &ui->style.base_scalars[UiScalar_animation_dampen], 1.0f, 128.0f);

	state->edit_buffer.data     = state->edit_buffer_storage;
	state->edit_buffer.capacity = ARRAY_COUNT(state->edit_buffer_storage);
	//ui_hover_tooltip(S("Text edit state box"));
	//ui_text_edit(S("Text Edit"), &state->edit_buffer);

	ui_row_checkbox(&builder, S("Debug Drawing"), &r1->debug_drawing_enabled);

	if (ui_row_button(&builder, S("Hot Reload Dog")))
	{
		reload_asset(asset_hash_from_string(S("gamedata/textures/dog.png")));
	}

	if (ui_row_button(&builder, S("Toggle Music")))
	{
		local_persist bool       is_playing = false;
		local_persist mixer_id_t music_handle;

		if (is_playing)
		{
			stop_sound(music_handle);
			is_playing = false;
		}
		else
		{
			music_handle = play_sound(&(play_sound_t){
				.waveform     = get_waveform_from_string(S("gamedata/audio/test.wav")),
				.volume       = 1.0f,
				.p            = make_v3(0, 0, 0),
				.min_distance = 100000.0f,
				.flags        = PLAY_SOUND_SPATIAL|PLAY_SOUND_FORCE_MONO|PLAY_SOUND_LOOPING,
			});
			is_playing = true;
		}
	}

	ui_row_slider    (&builder, S("Reverb Amount"), &mixer.reverb_amount, 0.0f, 1.0f);
	ui_row_slider    (&builder, S("Reverb Feedback"), &mixer.reverb_feedback, -1.0f, 1.0f);
	ui_row_slider_int(&builder, S("Delay Time"), &mixer.reverb_delay_time, 500, 2000);
	ui_row_slider    (&builder, S("Stereo Spread"), &mixer.reverb_stereo_spread, 0.0f, 2.0f);
	ui_row_slider    (&builder, S("Diffusion Angle"), &mixer.reverb_diffusion_angle, 0.0f, 90.0f);

	ui_row_slider(&builder, S("Filter Test"), &mixer.filter_test, -1.0f, 1.0f);

	ui_scrollable_region_end(scroll_region_id, builder.rect);
}

