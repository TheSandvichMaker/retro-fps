// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#if 0
fn_local void push_poly_wireframe(r_context_t *rc, map_t *map, map_poly_t *poly, v4_t color)
{
    uint16_t *indices   = map->indices          + poly->first_index;
    v3_t     *positions = map->vertex.positions + poly->first_vertex;

    for (size_t triangle_index = 0; triangle_index < poly->index_count / 3; triangle_index++)
    {
        v3_t a = positions[indices[3*triangle_index + 0]];
        v3_t b = positions[indices[3*triangle_index + 1]];
        v3_t c = positions[indices[3*triangle_index + 2]];

        r_immediate_line(rc, a, b, color);
        r_immediate_line(rc, a, c, color);
        r_immediate_line(rc, b, c, color);
    }
}

fn_local void push_brush_wireframe(r_context_t *rc, map_t *map, map_brush_t *brush, v4_t color)
{
    for (size_t poly_index = 0; poly_index < brush->plane_poly_count; poly_index++)
    {
        map_poly_t *poly = &map->polys[brush->first_plane_poly + poly_index];
        push_poly_wireframe(rc, map, poly, color);
    }
}

typedef enum editor_kind_t
{
	EDITOR_TIMINGS,
	EDITOR_LIGHTMAP,
	EDITOR_CONVEX_HULL,
	EDITOR_UI_DEMO,
	EDITOR_COUNT,
} editor_kind_t;

fn_local void ui_demo_proc(ui_window_t *);

typedef struct editor_state_t
{
	bool initialized;

	bool console_open;

    rect2_t *fullscreen_layout;
    float bar_openness;
	bool pin_bar;

    bool show_timings;

	bool hull_test_panel_enabled;
	convex_hull_debugger_t convex_hull_debugger;

	ui_demo_panel_t ui_demo;
} editor_state_t;

global editor_state_t editor = {
    .show_timings = false,

	.ui_demo = {
		.window = {
			.title     = Sc("UI Demo Panel"),
			.draw_proc = ui_demo_proc,
		},
	},
};

fn_local void show_profiler_stats(void)
{
	local_persist uint64_t cpu_freq = 0;

	if (cpu_freq == 0)
	{
		cpu_freq = estimate_cpu_timer_frequency(100);
	}

//	double total_time_ms = tsc_to_ms(profiler.end_tsc - profiler.start_tsc, cpu_freq);

	m_scoped_temp
	{
		/*
		uint64_t *sort_keys = m_alloc_array_nozero(temp, profiler_slots_count, uint64_t);

		for (size_t i = 1; i < profiler_slots_count; i += 1)
		{
			profiler_slot_t *slot = &profiler_slots[i];

			uint64_t key = i & 0xFFFF;          // low 16 bits is the index
			key |= (slot->exclusive_tsc) << 16; // uppwer 48 bits is tsc

			sort_keys[i - 1] = key;
		}

		radix_sort_u64(sort_keys, profiler_slots_count - 1);
		*/

		for (size_t i = 1; i < profiler_slots_count; i++)
		{
			//uint64_t key = sort_keys[j];

			//size_t i = key & 0xFFFF;
			profiler_slot_t *slot = &profiler_slots[i];

			double exclusive_ms  = tsc_to_ms(slot->exclusive_tsc, cpu_freq);
			double inclusive_ms  = tsc_to_ms(slot->inclusive_tsc, cpu_freq);
			double exclusive_pct = 0.0; //100.0*(exclusive_ms / total_time_ms);
			double inclusive_pct = 0.0; //100.0*(inclusive_ms / total_time_ms);

			ui_label(Sf("%-24s %.2f/%.2fms (%.2f/%.2f%%, %zu hits)", slot->tag, exclusive_ms, inclusive_ms, exclusive_pct, inclusive_pct, slot->hit_count));
		}
	}
}

fn_local void show_render_stats(void)
{
	m_scoped_temp
	{
		r1_stats_t stats = r1_report_stats(temp);

		UI_SCALAR(UI_SCALAR_TEXT_ALIGN_X, 0.5f)
		{
			double total = 0.0f;

			for (size_t i = 0; i < stats.timings_count; i++)
			{
				r1_timing_t *timing = &stats.timings[i];

				ui_label(Sf("%.*s: %.02fms", Sx(timing->identifier), 1000.0*timing->inclusive_time));
				total += timing->exclusive_time;
			}

			ui_label(Sf("total: %.02fms", 1000.0*total));
		}
	}
}

fn_local void fullscreen_update_and_render_top_editor_bar(void)
{
    rect2_t bar = rect2_cut_top(editor.fullscreen_layout, 32.0f);

    rect2_t collision_bar = bar;
    collision_bar.min.y -= 64.0f;

	if (editor.bar_openness > 0.0001f)
		collision_bar.min.y -= 128.0f;

    bool mouse_hover = ui_mouse_in_rect(collision_bar);
	mouse_hover |= ui->input.mouse_p.y >= collision_bar.max.y;
	mouse_hover |= editor.pin_bar;

    editor.bar_openness = ui_interpolate_f32(ui_id_pointer(&editor.bar_openness), mouse_hover ? 1.0f : 0.0f);

    if (editor.bar_openness > 0.0001f)
    {
        float height = rect2_height(bar);

        bar = rect2_add_offset(bar, make_v2(0.0f, (1.0f - editor.bar_openness)*height));

#if 0
        // TODO: Replace with UI draw calls
        r_immediate_rect2_filled(bar, ui_color(UI_COLOR_WINDOW_BACKGROUND));
        r_immediate_flush();
#endif

        rect2_t inner_bar = rect2_shrink(bar, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

		UI_SCALAR(UI_SCALAR_HOVER_LIFT, 0.0f)
        UI_PANEL(inner_bar)
        {
			ui_set_layout_direction(RECT2_CUT_LEFT);

			ui_checkbox(S("Pin"), &editor.pin_bar);

			ui_set_layout_direction(RECT2_CUT_RIGHT);

            typedef struct menu_t
            {
                string_t name;
                bool *toggle;
				ui_window_t *window;
            } menu_t;

            menu_t menus[] = {
                {
                    .name = S("Timings (F1)"),
                    .toggle = &editor.show_timings,
                },
                {
                    .name = S("Lightmap Editor (F2)"),
                    .window = &editor.lm_editor.window,
                },
                {
                    .name = S("Convex Hull Tester (F3)"),
                    .window = &editor.convex_hull_debugger.window,
                },
                {
                    .name = S("UI Demo (F4)"),
                    .window = &editor.ui_demo.window,
                },
            };

            for (int64_t i = ARRAY_COUNT(menus) - 1; i >= 0; i--)
            {
                menu_t *menu = &menus[i];
                
                bool active = menu->toggle ? *menu->toggle : menu->window->open;

                if (active) ui_push_color(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_BUTTON_ACTIVE));

                if (ui_button(menu->name))
                {
					if (menu->toggle) *menu->toggle = !*menu->toggle;
					if (menu->window)
					{
						ui_toggle_window_openness(menu->window);
					}
                }

                if (active) ui_pop_color(UI_COLOR_BUTTON_IDLE);
            }

            ui_label(S("  Menus: "));

            ui_label(Sf("Mouse Position: %.02f x %.02f", ui->input.mouse_p.x, ui->input.mouse_p.y));
			ui_label(Sf("Active Anims Count: %zu", ui->anim_list.active_count));
			ui_label(Sf("Sleepy Anims Count: %zu", ui->anim_list.sleepy_count));
			ui_label(Sf("Delta Time: %.02fms", 1000.0f*ui->dt));
			ui_label(Sf("UI Hover Time: %.02f", ui->hover_time_seconds));
			ui_label(Sf("UI Rect Count: %zu", ui->last_frame_ui_rect_count));
#if DREAM_SLOW
			ui_label(Sf("Hot ID: '%.*s'", Sx(UI_ID_GET_NAME(ui->hot))));
			ui_label(Sf("Active ID: '%.*s'", Sx(UI_ID_GET_NAME(ui->active))));
			ui_label(Sf("Hovered Panel: '%.*s'", Sx(UI_ID_GET_NAME(ui->hovered_panel))));
			ui_label(Sf("Hovered Widget: '%.*s'", Sx(UI_ID_GET_NAME(ui->hovered_widget))));
#endif
        }
    }
}

void update_and_render_in_game_editor(void)
{
	if (!editor.initialized)
	{
		editor.initialized = true;

		ui_add_window(&editor.lm_editor.window);
		editor.lm_editor.window.rect = rect2_from_min_dim(make_v2(32, 32), make_v2(512, 512));

		ui_add_window(&editor.convex_hull_debugger.window);
		editor.convex_hull_debugger.window.rect = rect2_from_min_dim(make_v2(64, 64), make_v2(512, 512));

		ui_add_window(&editor.ui_demo.window);
		editor.ui_demo.window.rect = rect2_from_min_dim(make_v2(96, 96), make_v2(512, 512));
	}

    if (ui_key_pressed(Key_f1, true))
	{
        editor.show_timings = !editor.show_timings;
	}

    if (ui_key_pressed(Key_f2, true))
	{
		ui_toggle_window_openness(&editor.lm_editor.window);
	}

    if (ui_key_pressed(Key_f3, true))
	{
		ui_toggle_window_openness(&editor.convex_hull_debugger.window);
	}

    if (ui_key_pressed(Key_f4, true))
	{
		ui_toggle_window_openness(&editor.ui_demo.window);
	}

	if (&editor.lm_editor.window.open)
		render_lm_editor();

	if (&editor.convex_hull_debugger.window.open)
		convex_hull_debugger_update_and_render(&editor.convex_hull_debugger);

#if 0
    int res_x, res_y;
    render->get_resolution(&res_x, &res_y);
#else
    int res_x = 1920;
	int res_y = 1080;
#endif

    rect2_t fullscreen_rect = {
        .min = { 0, 0 },
        .max = { (float)res_x, (float)res_y },
    };

    UI_SCALAR(UI_SCALAR_WIDGET_MARGIN, 0.0f)
    ui_panel_begin(fullscreen_rect);
	{
		editor.fullscreen_layout = ui_layout_rect();

		fullscreen_update_and_render_top_editor_bar();

		if (editor.show_timings)
		{
			show_profiler_stats();
			show_render_stats();
		}
	}

#if 0
	rect2_t r  = rect2_center_radius(ui->input.mouse_p, make_v2(1, 1));
	rect2_t r2 = rect2_from_min_dim(ui->input.mouse_p, make_v2(512, 1));
	ui_draw_rect(rect2_extend(r2, 1.0f), make_v4(0, 0, 0, 1));
	ui_draw_rect(rect2_extend(r, 1.0f), make_v4(0, 0, 0, 1));
	ui_draw_rect(r2, make_v4(1, 1, 1, 1));
	ui_draw_rect(r , make_v4(1, 1, 1, 1));
	ui_draw_text(ui->style.font, r.min, S("The Quick Brown Fox Jumped Over The Lazy Dog"));
#endif

	ui_panel_end();

	ui_process_windows();
}
#endif
