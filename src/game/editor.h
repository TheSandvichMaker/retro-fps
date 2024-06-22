#pragma once

#include "editor_convex_hull.h"
#include "editor_lightmap.h"
#include "editor_ui_test.h"
#include "editor_console.h"

typedef enum editor_window_kind_t
{
	EditorWindow_none,

	EditorWindow_lightmap,
	EditorWindow_convex_hull,
	EditorWindow_ui_test,
	EditorWindow_cvars,

	EditorWindow_COUNT,
} editor_window_kind_t;

typedef struct editor_window_t
{
	struct editor_window_t *next;
	struct editor_window_t *prev;

	string_storage_t(256) title;
	rect2_t rect;

	bool open;
	bool hovered;
	bool focused;

	ui_scrollable_region_t scroll_region;

	editor_window_kind_t kind;
} editor_window_t;

typedef struct editor_t
{
	arena_t arena;

	editor_window_t *first_window;
	editor_window_t * last_window;
	editor_window_t *focus_window;

	editor_window_t windows[EditorWindow_COUNT];

	editor_lightmap_state_t       lightmap;
	editor_convex_hull_debugger_t convex_hull;
	editor_ui_test_state_t        ui_test;
	cvar_window_state_t           cvar_window_state;
} editor_t;

fn void editor_init(editor_t *editor);
fn void editor_update_and_render(editor_t *editor);
