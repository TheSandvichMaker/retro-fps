#pragma once

typedef enum editor_split_direction_t
{
	EditorSplitDirection_horizontal,
	EditorSplitDirection_vertical,
} editor_split_direction_t;

typedef enum editor_panel_kind_t
{
	EditorPanel_none,
	EditorPanel_game_view,
	EditorPanel_lightmap_baker,
	EditorPanel_convex_hull_debugger,
	EditorPanel_COUNT,
} editor_panel_kind_t;

typedef struct editor_t
{
	arena_t arena;

	bool panel_shown[EditorPanel_COUNT];
} editor_t;
