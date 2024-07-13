#pragma once

typedef struct editor_texture_viewer_t
{
	ui_scrollable_region_t scroll_region;

	bool texture_view_open;
	bool buffer_view_open;

	rhi_texture_t current_texture;
} editor_texture_viewer_t;

fn void editor_texture_viewer_ui    (editor_texture_viewer_t *viewer, rect2_t rect);
fn void editor_texture_viewer_render(editor_texture_viewer_t *viewer, struct r1_view_t *view);
