#pragma once

enum { R1MaxUiRects = 4096 }; // TODO: Transient GPU allocator with CreatePlacedResource

typedef struct r1_state_t
{
	rhi_texture_t     white_texture;
	rhi_texture_srv_t white_texture_srv;

	rhi_texture_t     black_texture;
	rhi_texture_srv_t black_texture_srv;

	rect2i_t          unbounded_scissor_rect;

	uint32_t          shadow_map_resolution;
	rhi_texture_t     shadow_map;

	uint32_t          multisample_count;

	rhi_buffer_t      ui_rects;
	rhi_buffer_t      debug_lines;

	struct
	{
		rhi_pso_t sun_shadows;
		rhi_pso_t map;
		rhi_pso_t debug_lines;
		rhi_pso_t post_process;
		rhi_pso_t ui;
	} psos;

	struct
	{
		uint32_t      width;
		uint32_t      height;
		rhi_texture_t depth_stencil;
		rhi_texture_t rt_hdr;
	} window;

	struct
	{
		rhi_buffer_t positions;
		rhi_buffer_t uvs;
		rhi_buffer_t lightmap_uvs;
		rhi_buffer_t indices;
	} map;
} r1_state_t;

fn void r1_init(r1_state_t *r1);
fn void r1_update_window_resources(r1_state_t *r1, rhi_window_t window);
fn void r1_render_game_view(r1_state_t *r1, rhi_command_list_t *list, rhi_texture_t rt, r_view_t *view, struct world_t *world);
fn void r1_render_ui(r1_state_t *r1, rhi_command_list_t *list, rhi_texture_t rt, struct ui_render_command_list_t *ui_list);

typedef struct debug_line_t
{
	v3_t     start;
	uint32_t start_color;
	v3_t     end;
	uint32_t end_color;
} debug_line_t;

global uint32_t     debug_line_count;
global debug_line_t debug_lines[8192];

fn void draw_debug_line(v3_t start, v3_t end, v4_t start_color, v4_t end_color);
fn void draw_debug_cube(rect3_t bounds, v4_t color);
