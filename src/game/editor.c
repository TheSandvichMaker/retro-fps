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
	ui->has_focus        = true;
}

void editor_open_window(editor_t *editor, editor_window_t *window)
{
	window->open = true;
	editor_bring_window_to_front(editor, window);
	editor_focus_window         (editor, window);
}

void editor_close_window(editor_t *editor, editor_window_t *window)
{
	window->open = false;
	editor_send_window_to_back(editor, window);
	if (editor->focus_window == window && editor->last_window != window)
	{
		editor_focus_window(editor, editor->last_window);
	}
}

void editor_toggle_window_openness(editor_t *editor, editor_window_t *window)
{
	if (window->open)
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
	editor_window_t *hovered_window = NULL;

	uint8_t window_index = 0;

	for (editor_window_t *window = editor->first_window;
		 window;
		 window = window->next, window_index += 1)
	{
		if (!window->open)
			continue;

		ui_set_window_index(window_index);

		ui_id_t id = ui_id_pointer(window);

		ui_push_id(id);
		{
			// handle dragging and resizing first to remove frame delay
			bool is_being_dragged = ui_is_active(id);

			if (is_being_dragged)
			{
				v2_t new_p = add(ui->input.mouse_p, ui->drag_offset);
				window->rect = rect2_reposition_min(window->rect, new_p); 

				// compute total window rect
				rect2_t rect = window->rect;

				float   title_bar_height = ui_font(UiFont_header)->height + 2.0f*ui_scalar(UiScalar_text_margin) + 4.0f;
				rect2_t title_bar = rect2_add_top(rect, title_bar_height);

				rect2_t total = rect2_union(title_bar, rect);

				float w = rect2_width (total);
				float h = rect2_height(total);

				rect2_t restrict_rect = ui->ui_area;
				restrict_rect = rect2_shrink2   (restrict_rect, 0.5f*w, 0.5f*h);
				restrict_rect = rect2_add_offset(restrict_rect, ui->drag_anchor);

				ui->restrict_mouse_rect = restrict_rect;
				ui->cursor              = Cursor_none;
			}

			ui_id_t id_n  = ui_id(S("tray_id_n"));
			ui_id_t id_e  = ui_id(S("tray_id_e"));
			ui_id_t id_s  = ui_id(S("tray_id_s"));
			ui_id_t id_w  = ui_id(S("tray_id_w"));
			ui_id_t id_ne = ui_id(S("tray_id_ne"));
			ui_id_t id_nw = ui_id(S("tray_id_nw"));
			ui_id_t id_se = ui_id(S("tray_id_se"));
			ui_id_t id_sw = ui_id(S("tray_id_sw"));

			if (ui_is_hot(id_n))  { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_ns; };
			if (ui_is_hot(id_e))  { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_ew; };
			if (ui_is_hot(id_s))  { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_ns; };
			if (ui_is_hot(id_w))  { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_ew; };
			if (ui_is_hot(id_ne)) { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_nesw; };
			if (ui_is_hot(id_nw)) { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_nwse; };
			if (ui_is_hot(id_se)) { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_nwse; };
			if (ui_is_hot(id_sw)) { ui->cursor_reset_delay = 1; ui->cursor = Cursor_resize_nesw; };

			ui_rect_edge_t tray_region = 0;

			if (ui_is_active(id_n))  tray_region = UiRectEdge_n;
			if (ui_is_active(id_e))  tray_region = UiRectEdge_e;
			if (ui_is_active(id_s))  tray_region = UiRectEdge_s;
			if (ui_is_active(id_w))  tray_region = UiRectEdge_w;
			if (ui_is_active(id_ne)) tray_region = UiRectEdge_n|UiRectEdge_e;
			if (ui_is_active(id_nw)) tray_region = UiRectEdge_n|UiRectEdge_w;
			if (ui_is_active(id_se)) tray_region = UiRectEdge_s|UiRectEdge_e;
			if (ui_is_active(id_sw)) tray_region = UiRectEdge_s|UiRectEdge_w;

			if (tray_region)
			{
				v2_t delta = sub(ui->input.mouse_p, ui->input.mouse_p_on_lmb);

				window->rect = ui->resize_original_rect;
				if (tray_region & UiRectEdge_e) window->rect = rect2_extend_right(window->rect,  delta.x);
				if (tray_region & UiRectEdge_w) window->rect = rect2_extend_left (window->rect, -delta.x);
				if (tray_region & UiRectEdge_n) window->rect = rect2_extend_up   (window->rect,  delta.y);
				if (tray_region & UiRectEdge_s) window->rect = rect2_extend_down (window->rect, -delta.y);

				rect2_t min_rect = rect2_from_min_dim(window->rect.min, make_v2(64, 64));
				window->rect = rect2_union(window->rect, min_rect);
			}

			// ---

			rect2_t rect = window->rect;

			/*
			float held_offset = 0.0f;

			if (is_being_dragged)
			{
				held_offset = 8.0f;
			}

			held_offset = ui_interpolate_f32(ui_id(S("held_offset")), held_offset);

			rect = rect2_add_offset(rect, make_v2(0, held_offset));
			*/

			float   title_bar_height = ui_font(UiFont_header)->height + 2.0f*ui_scalar(UiScalar_text_margin) + 4.0f;
			rect2_t title_bar = rect2_add_top(rect, title_bar_height);

			rect2_t total = rect2_union(title_bar, rect);

			// drag and resize behaviour

			float tray_width = 8.0f;

			rect2_t interact_total = rect2_extend(total, 0.5f*tray_width);
			ui_interaction_t interaction = ui_default_widget_behaviour(id, interact_total);

			if (ui_mouse_in_rect(interact_total))
			{
				hovered_window = window;
			}

			if (interaction & UI_PRESSED)
			{
				ui->drag_offset = sub(window->rect.min, ui->input.mouse_p);
			}

			rect2_t tray_init = interact_total;
			rect2_t tray_e  = rect2_cut_right (&tray_init, tray_width);
			rect2_t tray_w  = rect2_cut_left  (&tray_init, tray_width);
			rect2_t tray_n  = rect2_cut_top   (&tray_init, tray_width);
			rect2_t tray_s  = rect2_cut_bottom(&tray_init, tray_width);
			rect2_t tray_ne = rect2_cut_top   (&tray_e,    tray_width);
			rect2_t tray_nw = rect2_cut_top   (&tray_w,    tray_width);
			rect2_t tray_se = rect2_cut_bottom(&tray_e,    tray_width);
			rect2_t tray_sw = rect2_cut_bottom(&tray_w,    tray_width);

			ui_interaction_t tray_interaction = 0;
			tray_interaction |= ui_default_widget_behaviour(id_n,  tray_n);
			tray_interaction |= ui_default_widget_behaviour(id_e,  tray_e);
			tray_interaction |= ui_default_widget_behaviour(id_s,  tray_s);
			tray_interaction |= ui_default_widget_behaviour(id_w,  tray_w);
			tray_interaction |= ui_default_widget_behaviour(id_ne, tray_ne);
			tray_interaction |= ui_default_widget_behaviour(id_nw, tray_nw);
			tray_interaction |= ui_default_widget_behaviour(id_se, tray_se);
			tray_interaction |= ui_default_widget_behaviour(id_sw, tray_sw);

			if (tray_interaction & UI_PRESSED)
			{
				ui->resize_original_rect = window->rect;
			}

			// draw window

			rect2_t title_bar_minus_outline = title_bar;
			title_bar_minus_outline.max.y -= 2.0f;

			string_t title = string_from_storage(window->title);

			bool has_focus = /*ui_has_focus() &&*/ (window == editor->focus_window);

			float focus_t = ui_interpolate_f32(ui_id(S("focus")), has_focus);

			float shadow_amount = has_focus ? 0.25f : 0.15f;

			if (is_being_dragged)
			{
				shadow_amount = 0.05f;
			}

			shadow_amount = ui_interpolate_f32(ui_id(S("shadow")), shadow_amount);

			ui_style_color_t title_bar_color_id = UiColor_window_title_bar;

			if (is_being_dragged)
			{
				title_bar_color_id = UiColor_window_title_bar_hot;
			}

			v4_t  title_bar_color           = ui_color(title_bar_color_id);
			float title_bar_luma            = luminance(title_bar_color.xyz);
			v4_t  title_bar_color_greyscale = make_v4(title_bar_luma, title_bar_luma, title_bar_luma, 1.0f);

			v4_t  interpolated_title_bar_color = v4_lerps(title_bar_color_greyscale, title_bar_color, focus_t);
			interpolated_title_bar_color = ui_interpolate_v4(ui_id(S("title_bar_color")), interpolated_title_bar_color);

			ui_draw_rect_roundedness_shadow(rect2_shrink(total, 1.0f), make_v4(0, 0, 0, 0), make_v4(5, 5, 5, 5), shadow_amount, 32.0f);
			ui_draw_rect_roundedness(title_bar, interpolated_title_bar_color, make_v4(2, 0, 2, 0));
			ui_draw_rect_roundedness(rect, ui_color(UiColor_window_background), make_v4(0, 2, 0, 2));
			ui_push_clip_rect(title_bar, true);
			ui_draw_text(ui_font(UiFont_header), ui_text_center_p(ui_font(UiFont_header), title_bar_minus_outline, title), title);
			ui_pop_clip_rect();
			ui_draw_rect_roundedness_outline(total, ui_color(UiColor_window_outline), make_v4(2, 2, 2, 2), 2.0f);

			// handle window contents

			// float margin = ui_scalar(UiScalar_widget_margin);
			// rect = rect2_shrink(rect, margin);
			// rect = rect2_pillarbox(rect, ui_scalar(UiScalar_window_margin));

			ui_push_clip_rect(rect, true);

			window->focused = has_focus;

			switch (window->kind)
			{
				case EditorWindow_lightmap:
				{
					editor_do_lightmap_window(&editor->lightmap, window, window->rect);
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

			ui_pop_clip_rect();

			window->hovered = false;
		}
		ui_pop_id();
	}

	if (hovered_window)
	{
		ui->hovered = true;
		hovered_window->hovered = true;
	}

	if (ui_button_pressed(UiButton_any, false))
	{
		if (hovered_window)
		{
			editor_bring_window_to_front(editor, hovered_window);
		}

		editor->focus_window = hovered_window;
	}
}

void editor_init(editor_t *editor)
{
	v2_t at = make_v2(32, 32);

	for (size_t i = 0; i < EditorWindow_COUNT; i++)
	{
		editor_add_window(editor, &editor->windows[i]);
		editor->windows[i].rect = rect2_from_min_dim(at, make_v2(512, 512));

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

void editor_update_and_render(editor_t *editor)
{
	/*
    if (ui_key_pressed(Key_f1, true))
	{
        editor.show_timings = !editor.show_timings;
	}
	*/

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

	editor_process_windows(editor);
}
