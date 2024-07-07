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
