#pragma once

#include "shaders/gen/shaders.h"

// #include "r1_ui.h"

typedef enum r_parameter_slot_t
{
	R1ParameterSlot_draw = 0,
	R1ParameterSlot_pass = 1,
	R1ParameterSlot_view = 2,
} r_parameter_slot;

enum { R1MaxUiRects = 16 << 10 }; // TODO: Transient GPU allocator with CreatePlacedResource
enum { R1MaxRegions = 512  };

typedef struct r1_view_render_targets_t
{
	rhi_texture_t depth_stencil;
	rhi_texture_t rt_hdr;
	rhi_texture_t rt_hdr_resolved;
	// rhi_texture_t rt_ui_heatmap;
	rhi_texture_t bloom_targets[8];
	rhi_texture_t rt_window;
} r1_view_render_targets_t;

typedef struct r1_scene_parameters_t
{
    v3_t              sun_direction;
    v3_t              sun_color;

    v3_t              fog_offset;
    v3_t              fog_dim;
    float             fog_density;
    float             fog_absorption;
    float             fog_scattering;
    float             fog_phase_k;
    v3_t              fog_ambient_inscattering;
} r1_scene_parameters_t;

typedef struct r1_view_t
{
	uint32_t render_w;
	uint32_t render_h;

	r1_view_render_targets_t targets;

	// TODO: Either honor or remove these
    bool no_shadows      : 1;
    bool no_post_process : 1;

    v3_t camera_p;

    m4x4_t view_matrix;
    m4x4_t proj_matrix;

    rect2_t clip_rect;

    r1_scene_parameters_t scene;
} r1_view_t;

fn r1_view_t r1_create_view(rhi_window_t window);

typedef struct r1_state_t
{
	arena_t arena;
	arena_t *push_constants_arena;

	bool debug_drawing_enabled;

	rhi_texture_t     white_texture;
	rhi_texture_srv_t white_texture_srv;

	rhi_texture_t     black_texture;
	rhi_texture_srv_t black_texture_srv;

	rhi_texture_t     missing_texture;
	rhi_texture_srv_t missing_texture_srv;

	rhi_texture_t     blue_noise[8];
	rhi_texture_srv_t blue_noise_srv[8];

	rect2i_t          unbounded_scissor_rect;

	uint32_t          shadow_map_resolution;
	rhi_texture_t     shadow_map;

	uint32_t          multisample_count;

	// rhi_buffer_t      ui_rects;
	rhi_buffer_t      debug_lines;

	uint32_t indirect_args_capacity;
	uint32_t indirect_args_count;

	rhi_buffer_t args;
	rhi_indirect_draw_t *args_at;

	uint64_t timestamp_frequency;
	uint32_t next_region_index;
	string_storage_t(256) region_identifiers[R1MaxRegions];

	uint32_t frame_index;

	rhi_pso_t psos[DfPso_COUNT];

	struct
	{
		rhi_buffer_t positions;
		rhi_buffer_t uvs;
		rhi_buffer_t lightmap_uvs;
		rhi_buffer_t indices;
	} map;
} r1_state_t;

global thread_local r1_state_t *r1;
fn void r1_equip  (r1_state_t *state);
fn void r1_unequip(void);

fn r1_state_t *r1_make(void);

fn rhi_pso_t r1_create_fullscreen_pso(string_t debug_name, rhi_shader_bytecode_t ps, pixel_format_t pf);

fn void r1_begin_frame(void);
fn void r1_finish_recording_draw_streams(void);
fn void r1_render_game_view(rhi_command_list_t *list, r1_view_t *view, map_t *map);
fn void r1_render_ui(rhi_command_list_t *list, r1_view_t *view, ui_render_command_list_t *ui_list);
fn void r1_set_pso(rhi_command_list_t *list, df_pso_ident_t pso);

typedef struct r1_timing_t
{
	string_t identifier;
	double   inclusive_time;
	double   exclusive_time;
} r1_timing_t;

typedef struct r1_stats_t
{
	uint32_t     timings_count;
	r1_timing_t *timings;
} r1_stats_t; 

fn r1_stats_t r1_report_stats(arena_t *arena);

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

typedef enum r1_render_bucket_t
{
	R1RenderBucket_opaque,
	R1RenderBucket_translucent,

	R1RenderBucket_COUNT,
} r1_render_bucket_t;

typedef struct r1_push_constants_t
{
	void    *host;
	uint32_t offset;
	uint32_t size;
} r1_push_constants_t;

#define r1_set_draw_params(list, params) shader_set_params(list, R1ParameterSlot_draw, params)
#define r1_set_pass_params(list, params) shader_set_params(list, R1ParameterSlot_pass, params)
#define r1_set_view_params(list, params) shader_set_params(list, R1ParameterSlot_view, params)

