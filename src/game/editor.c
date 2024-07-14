#include "editor_lightmap.c"
#include "editor_convex_hull.c"
#include "editor_ui_test.c"
#include "editor_texture_viewer.c"

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

				case EditorWindow_texture_viewer:
				{
					editor_texture_viewer_ui(&editor->texture_viewer, window->window.rect);
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
		editor->windows[i].kind = (editor_window_kind_t)i;

		at = add(at, make_v2(32, 32));
	}

	string_into_storage(editor->windows[EditorWindow_lightmap].title,       S("Lightmap Editor"));
	string_into_storage(editor->windows[EditorWindow_convex_hull].title,    S("Convex Hull Debugger"));
	string_into_storage(editor->windows[EditorWindow_ui_test].title,        S("UI Test Window"));
	string_into_storage(editor->windows[EditorWindow_cvars].title,          S("Console Variables"));
	string_into_storage(editor->windows[EditorWindow_texture_viewer].title, S("Texture Viewer"));

	editor_init_lightmap_stuff(&editor->lightmap);
	editor_init_convex_hull_debugger(&editor->convex_hull);
	editor_init_cvar_window(&editor->cvar_window_state);
}

fn_local int sort_profile_slots(const void *l, const void *r, void *user_data)
{
	(void)user_data;

	const profiler_slot_t *slot_l = *(profiler_slot_t **)l;
	const profiler_slot_t *slot_r = *(profiler_slot_t **)r;

	return slot_l->display_exclusive_ms < slot_r->display_exclusive_ms;
}

void editor_show_timings(editor_t *editor)
{
    (void)editor;

    arena_t *temp = m_get_temp_scope_begin(NULL, 0);

    r1_stats_t stats = r1_report_stats(temp);

    ui_set_layer((ui_layer_t){ .layer = 255 });

    rect2_t full_area = ui->ui_area;
	rect2_cut_from_top(full_area, ui_sz_pct(0.333f), &full_area, NULL);

	ui_draw_rect(full_area, make_v4(0, 0, 0, 0.75f));

    float one_third = ui_size_to_width(full_area, ui_sz_pct(0.33333f));

	//------------------------------------------------------------------------

    rect2_t gpu_area;
    rect2_cut_from_left(full_area, ui_sz_pix(one_third), &gpu_area, &full_area);

    {
        ui_row_builder_t builder = ui_make_row_builder(ui_scrollable_region_begin_ex(&editor->gpu_stats_scroll_region, gpu_area, UiScrollableRegionFlags_scroll_vertical));
		ui_push_sub_layer();

        ui_row_header(&builder, S("GPU Timings"));

		double total = 0.0f;

		for (size_t i = 0; i < stats.timings_count; i++)
		{
			r1_timing_t *timing = &stats.timings[i];

			ui_row_labels2(&builder, Sf("%cs:", timing->identifier), Sf("%.02fms", 1000.0*timing->inclusive_time));
			total += timing->exclusive_time;
		}

		ui_row_labels2(&builder, S("total:"), Sf("%.02fms", 1000.0*total));

		ui_pop_sub_layer();
		ui_scrollable_region_end(&editor->gpu_stats_scroll_region, builder.rect);
    }

	//------------------------------------------------------------------------

    rect2_t cpu_area;
    rect2_cut_from_left(full_area, ui_sz_pix(one_third), &cpu_area, &full_area);

	static int stupid = 0;

	{
		profiler_slot_t **sorted_slots = m_alloc_array_nozero(temp, profiler_slots_count - 1, profiler_slot_t *);

		for (size_t i = 1; i < profiler_slots_count; i += 1)
		{
			sorted_slots[i - 1] = &profiler_slots_read[i];
		}

		// Sorting makes things too jittery...
		// merge_sort_array(sorted_slots, profiler_slots_count - 1, sort_profile_slots, NULL);

        static uint64_t cpu_freq = 0;

        if (cpu_freq == 0)
        {
            cpu_freq = os_estimate_cpu_timer_frequency(100);
        }

        ui_row_builder_t builder = ui_make_row_builder(ui_scrollable_region_begin_ex(&editor->cpu_stats_scroll_region, cpu_area, UiScrollableRegionFlags_scroll_vertical));
		ui_push_sub_layer();

        ui_row_header(&builder, S("CPU Timings"));

		if (!editor->profiler_stats)
		{
			editor->profiler_stats = m_alloc_array(&editor->arena, profiler_slots_count, profiler_stat_t);
		}

		for (size_t i = 0; i < profiler_slots_count - 1; i++)
		{
			profiler_slot_t *slot = sorted_slots[i];
			// profiler_stat_t *stat = &editor->profiler_stats[slot_index_from_slot(profiler_slots_read, slot)];

			double exclusive_ms  = slot->display_exclusive_ms;
			double inclusive_ms  = slot->display_inclusive_ms;
			double exclusive_pct = 0.0; //100.0*(exclusive_ms / total_time_ms);
			double inclusive_pct = 0.0; //100.0*(inclusive_ms / total_time_ms);

			ui_row_labels2(&builder, Sf("%s", slot->tag), Sf("%.2f/%.2fms (%.2f/%.2f%%, %zu hits)", exclusive_ms, inclusive_ms, exclusive_pct, inclusive_pct, slot->hit_count));
		}

		ui_pop_sub_layer();
		ui_scrollable_region_end(&editor->cpu_stats_scroll_region, builder.rect);
	}

	stupid += 1;

	//------------------------------------------------------------------------

    rect2_t gpu_alloc_area;
    rect2_cut_from_left(full_area, ui_sz_pix(one_third), &gpu_alloc_area, &full_area);

    {
        ui_row_builder_t builder = ui_make_row_builder(ui_scrollable_region_begin_ex(&editor->gpu_alloc_stats_scroll_region, gpu_alloc_area, UiScrollableRegionFlags_scroll_vertical));
		ui_push_sub_layer();

        ui_row_header(&builder, S("GPU Allocations"));

		rhi_allocation_stats_t alloc_stats;
		rhi_get_allocation_stats(&alloc_stats);

		ui_row_labels2(&builder, S("Transient Memory Used:"), string_format_human_readable_bytes(temp, alloc_stats.transient_memory_used_bytes));
		ui_row_labels2(&builder, S("Transient Nodes Size:"), string_format_human_readable_bytes(temp, alloc_stats.transient_nodes_size));
		ui_row_labels2(&builder, S("Transient Nodes Used:"), Sf("%llu", alloc_stats.transient_nodes_used));
		ui_row_labels2(&builder, S("Free Transient Nodes:"), Sf("%llu", alloc_stats.free_transient_nodes));
		ui_row_labels2(&builder, S("Scratch Arena Used:"), string_format_human_readable_bytes(temp, alloc_stats.scratch_arena_used_bytes));
		ui_row_labels2(&builder, S("Upload Arena Used:"), string_format_human_readable_bytes(temp, alloc_stats.upload_arena_used_bytes));
		ui_row_labels2(&builder, S("Ring Buffer Slots Used:"), Sf("%llu/%llu", alloc_stats.ring_buffer_slots_used, alloc_stats.ring_buffer_slots_capacity));
		ui_row_labels2(&builder, S("Ring Buffer Bytes Used:"), Sf("%cs/%cs", string_format_human_readable_bytes(temp, alloc_stats.ring_buffer_bytes_used), string_format_human_readable_bytes(temp, alloc_stats.ring_buffer_bytes_capacity)));
		ui_row_labels2(&builder, S("CBV/SRV/UAV Descriptors Used:"), Sf("%llu", alloc_stats.persistent_cbv_srv_uav_count));
		ui_row_labels2(&builder, S("RTV Descriptors Used:"), Sf("%llu", alloc_stats.persistent_rtv_count));
		ui_row_labels2(&builder, S("DSV Descriptors Used:"), Sf("%llu", alloc_stats.persistent_dsv_count));

		ui_pop_sub_layer();
		ui_scrollable_region_end(&editor->gpu_alloc_stats_scroll_region, builder.rect);
    }

	//------------------------------------------------------------------------

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

    if (ui_key_pressed(Key_f6, true))
	{
		editor_toggle_window_openness(editor, &editor->windows[EditorWindow_texture_viewer]);
	}

    if (editor->show_timings)
    {
        editor_show_timings(editor);
    }

	editor_process_windows(editor);
	editor_texture_viewer_render(&editor->texture_viewer, NULL);
}
