#include "editor_lightmap.c"
#include "editor_convex_hull.c"
#include "editor_ui_test.c"

void editor_add_window(editor_t *editor, editor_window_t *window)
{
	dll_push_back(editor->first_window, editor->last_window, window);
}

void editor_remove_window(editor_t *editor, editor_window_t *window)
{
	dll_remove(editor->first_window, editor->last_window, window);
}

void editor_bring_window_to_front(editor_t *editor, editor_window_t *window)
{
	dll_remove   (editor->first_window, editor->last_window, window);
	dll_push_back(editor->first_window, editor->last_window, window);
}

void editor_send_window_to_back(editor_t *editor, editor_window_t *window)
{
	dll_remove    (editor->first_window, editor->last_window, window);
	dll_push_front(editor->first_window, editor->last_window, window);
}

void editor_focus_window(editor_t *editor, editor_window_t *window)
{
	editor->focus_window = window;
	ui_gain_focus(ui_id_pointer(window));
}

void editor_open_window(editor_t *editor, editor_window_t *window)
{
	window->window.open = true;

	editor_bring_window_to_front(editor, window);
	editor_focus_window         (editor, window);
}

void editor_close_window(editor_t *editor, editor_window_t *window)
{
	window->window.open = false;
	editor_send_window_to_back(editor, window);

	ui_remove_from_responder_chain(ui_id_pointer(window));

	for (editor_window_t *w = editor->last_window; w; w = w->prev)
	{
		if (w->window.open)
		{
			editor_focus_window(editor, w);
			break;
		}
	}
}

void editor_toggle_window_openness(editor_t *editor, editor_window_t *window)
{
	if (window->window.open)
	{
		editor_close_window(editor, window);
	}
	else
	{
		editor_open_window(editor, window);
	}
}

void editor_process_windows(editor_t *editor)
{
	uint8_t window_index = 0;

	for (editor_window_t *window = editor->first_window;
		 window;
		 window = window->next, window_index += 1)
	{
		ui_id_t window_id = ui_id_pointer(window);

		ui_set_layer((ui_layer_t){ .layer = window_index });

		if (ui_window_begin(window_id, &window->window, string_from_storage(window->title)))
		{
			switch (window->kind)
			{
				case EditorWindow_lightmap:
				{
					editor_do_lightmap_window(&editor->lightmap, window, window->window.rect);
				} break;

				case EditorWindow_convex_hull:
				{
					editor_do_convex_hull_debugger_window(&editor->convex_hull, window);
				} break;

				case EditorWindow_ui_test:
				{
					editor_do_ui_test_window(&editor->ui_test, window);
				} break;

				case EditorWindow_cvars:
				{
					editor_do_cvar_window(&editor->cvar_window_state, window);
				} break;
			}

			ui_window_end(window_id, &window->window);
		}
	}

	for (editor_window_t *window = editor->first_window;
		 window;
		 window = window->next)
	{
		if (window->window.focused)
		{
			editor_bring_window_to_front(editor, window);
			editor->focus_window = window;
			break;
		}
	}
}

void editor_init(editor_t *editor)
{
	v2_t at = make_v2(32, 32);

	for (size_t i = 0; i < EditorWindow_COUNT; i++)
	{
		editor_add_window(editor, &editor->windows[i]);
		editor->windows[i].window.rect = rect2_from_min_dim(at, make_v2(512, 512));

		at = add(at, make_v2(32, 32));
	}

	string_into_storage(editor->windows[EditorWindow_lightmap].title,    S("Lightmap Editor"));
	string_into_storage(editor->windows[EditorWindow_convex_hull].title, S("Convex Hull Debugger"));
	string_into_storage(editor->windows[EditorWindow_ui_test].title,     S("UI Test Window"));
	string_into_storage(editor->windows[EditorWindow_cvars].title,       S("Console Variables"));

	editor->windows[EditorWindow_lightmap].kind    = EditorWindow_lightmap;
	editor->windows[EditorWindow_convex_hull].kind = EditorWindow_convex_hull;
	editor->windows[EditorWindow_ui_test].kind     = EditorWindow_ui_test;
	editor->windows[EditorWindow_cvars].kind       = EditorWindow_cvars;

	editor_init_lightmap_stuff(&editor->lightmap);
	editor_init_convex_hull_debugger(&editor->convex_hull);
	editor_init_cvar_window(&editor->cvar_window_state);
}

void editor_show_timings(editor_t *editor)
{
    (void)editor;

    arena_t *temp = m_get_temp_scope_begin(NULL, 0);

    r1_stats_t stats = r1_report_stats(temp);

    ui_set_layer((ui_layer_t){ .layer = 255 });

    rect2_t full_area = ui->ui_area;

    float one_third = ui_size_to_width(full_area, ui_sz_pct(0.33333f));

    rect2_t gpu_area;
    rect2_cut_from_left(full_area, ui_sz_pix(one_third), &gpu_area, &full_area);

    {
        ui_row_builder_t builder = ui_make_row_builder(gpu_area);

        ui_row_header(&builder, S("GPU Timings"));

        UI_Scalar(UiScalar_label_align_x, 0.5f)
        {
            double total = 0.0f;

            for (size_t i = 0; i < stats.timings_count; i++)
            {
                r1_timing_t *timing = &stats.timings[i];

                ui_row_label(&builder, Sf("%.*s: %.02fms", Sx(timing->identifier), 1000.0*timing->inclusive_time));
                total += timing->exclusive_time;
            }

            ui_row_label(&builder, Sf("total: %.02fms", 1000.0*total));
        }
    }

    rect2_t cpu_area;
    rect2_cut_from_left(full_area, ui_sz_pix(one_third), &cpu_area, &full_area);

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

        static uint64_t cpu_freq = 0;

        if (cpu_freq == 0)
        {
            cpu_freq = os_estimate_cpu_timer_frequency(100);
        }

        ui_row_builder_t builder = ui_make_row_builder(cpu_area);

        ui_row_header(&builder, S("CPU Timings"));

        UI_Scalar(UiScalar_label_align_x, 0.5f)
		for (size_t i = 1; i < profiler_slots_count; i++)
		{
			//uint64_t key = sort_keys[j];

			//size_t i = key & 0xFFFF;
			profiler_slot_t *slot = &profiler_slots_read[i];

			double exclusive_ms  = tsc_to_ms(slot->exclusive_tsc, cpu_freq);
			double inclusive_ms  = tsc_to_ms(slot->inclusive_tsc, cpu_freq);
			double exclusive_pct = 0.0; //100.0*(exclusive_ms / total_time_ms);
			double inclusive_pct = 0.0; //100.0*(inclusive_ms / total_time_ms);

			ui_row_label(&builder, Sf("%-24s %.2f/%.2fms (%.2f/%.2f%%, %zu hits)", slot->tag, exclusive_ms, inclusive_ms, exclusive_pct, inclusive_pct, slot->hit_count));
		}
	}

    m_scope_end(temp);
}

void editor_update_and_render(editor_t *editor)
{
    if (ui_key_pressed(Key_f1, true))
	{
        editor->show_timings = !editor->show_timings;
	}

    if (ui_key_pressed(Key_f2, true))
	{
		editor_toggle_window_openness(editor, &editor->windows[EditorWindow_lightmap]);
	}

    if (ui_key_pressed(Key_f3, true))
	{
		editor_toggle_window_openness(editor, &editor->windows[EditorWindow_convex_hull]);
	}

    if (ui_key_pressed(Key_f4, true))
	{
		editor_toggle_window_openness(editor, &editor->windows[EditorWindow_ui_test]);
	}

    if (ui_key_pressed(Key_f5, true))
	{
		editor_toggle_window_openness(editor, &editor->windows[EditorWindow_cvars]);
	}

    if (editor->show_timings)
    {
        editor_show_timings(editor);
    }

	editor_process_windows(editor);
}
