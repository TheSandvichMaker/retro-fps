#ifndef DREAM_UI_H
#define DREAM_UI_H

#include "core/api_types.h"

#include "platform.h"
#include "font.h"

//
//
//

#define UI_SLOW (1 && DREAM_SLOW)

//
// IDs
//

typedef struct ui_id_t
{
	uint64_t value;
#if DREAM_SLOW
	string_storage_t(64) name;
#endif
} ui_id_t;

#define UI_ID_NULL (ui_id_t){0}

fn ui_id_t ui_id        (string_t string);
fn ui_id_t ui_child_id  (ui_id_t parent, string_t string);
fn ui_id_t ui_id_pointer(void *pointer);
fn ui_id_t ui_id_u64    (uint64_t u64);

fn_local bool ui_id_valid(ui_id_t id)
{
	return id.value != 0;
}

fn void    ui_set_next_id(ui_id_t id);
fn void    ui_push_id    (ui_id_t id);
fn void    ui_pop_id     (void);

//
// Input
//

typedef struct ui_input_t
{
	float dt;

	bool app_has_focus;
	bool alt_down;
	bool shift_down;
	bool ctrl_down;

	platform_event_t *events;

	platform_mouse_buttons_t mouse_buttons_down;
	platform_mouse_buttons_t mouse_buttons_pressed;
	platform_mouse_buttons_t mouse_buttons_released;
	float mouse_wheel;
	v2_t  mouse_p;
	v2_t  mouse_pressed_p;
	v2_t  mouse_dp;

	dynamic_string_t text;

	platform_cursor_t cursor;
	int               cursor_reset_delay;
} ui_input_t;

fn void ui_submit_mouse_buttons(platform_mouse_buttons_t buttons, bool pressed);
fn void ui_submit_mouse_wheel  (float wheel_delta);
fn void ui_submit_mouse_p      (v2_t p);
fn void ui_submit_mouse_dp     (v2_t dp);
fn void ui_submit_text         (string_t text);

fn bool ui_mouse_buttons_down    (platform_mouse_buttons_t buttons);
fn bool ui_mouse_buttons_pressed (platform_mouse_buttons_t buttons);
fn bool ui_mouse_buttons_released(platform_mouse_buttons_t buttons);

//
// Panels
//

typedef uint32_t ui_panel_flags_t;

typedef struct ui_panel_t
{
	union
	{
		struct ui_panel_t *parent;
		struct ui_panel_t *next_free;
	};

	ui_id_t          id;

	ui_panel_flags_t flags;

	rect2_cut_side_t layout_direction;
	rect2_t          rect_init;
	rect2_t          rect_layout;
} ui_panel_t;

typedef struct ui_panels_t
{
	ui_panel_t *current_panel;
	ui_panel_t *first_free_panel;
} ui_panels_t;

fn ui_panel_t *ui_push_panel(ui_id_t id, rect2_t rect, ui_panel_flags_t flags);
fn void        ui_pop_panel (void);
fn ui_panel_t *ui_panel     (void);

fn rect2_t *ui_layout_rect         (void);
fn void     ui_set_layout_direction(rect2_cut_side_t side);
fn void     ui_set_next_rect       (rect2_t rect);
fn float    ui_divide_space        (float item_count);

// layout helpers
fn float    ui_widget_padding      (void);
fn bool     ui_override_rect       (rect2_t *rect);
fn rect2_t  ui_default_label_rect  (font_atlas_t *font, string_t label);

typedef uint32_t ui_rect_edge_t;
typedef enum ui_rect_edge_enum_t
{
	UI_RECT_EDGE_N = (1 << 0),
	UI_RECT_EDGE_E = (1 << 1),
	UI_RECT_EDGE_S = (1 << 2),
	UI_RECT_EDGE_W = (1 << 3),
} ui_rect_edge_enum_t;

//
// Windows
//

typedef struct ui_window_t ui_window_t;
typedef void (*ui_window_proc_t)(ui_window_t *window);

struct ui_window_t
{
	struct ui_window_t *next;
	struct ui_window_t *prev;

	string_storage_t(256) title;
	rect2_t rect;

	bool open;
	bool hovered;
	bool focused;

	void            *user_data;
	ui_window_proc_t draw_proc;
};

typedef struct ui_windows_t
{
	ui_window_t *first_window;
	ui_window_t *last_window;
	ui_window_t *focus_window;
} ui_windows_t;

fn void ui_add_window            (ui_window_t *window);
fn void ui_remove_window         (ui_window_t *window);
fn void ui_bring_window_to_front (ui_window_t *window);
fn void ui_send_window_to_back   (ui_window_t *window);
fn void ui_focus_window          (ui_window_t *window);
fn void ui_open_window           (ui_window_t *window);
fn void ui_close_window          (ui_window_t *window);
fn void ui_toggle_window_openness(ui_window_t *window);

fn void ui_process_windows(void);

//
// Style
//

typedef struct ui_anim_t
{
	ui_id_t  id;
	uint64_t last_touched_frame_index;

	float length_limit;
	float c_t;
	float c_v;
	v4_t  t_target;
	v4_t  t_current;
	v4_t  t_velocity;
} ui_anim_t;

#define UI_STYLE_STACK_COUNT 32

typedef enum ui_style_scalar_t
{
	UI_SCALAR_TOOLTIP_DELAY,

    UI_SCALAR_ANIMATION_RATE,

	UI_SCALAR_ANIMATION_STIFFNESS,
	UI_SCALAR_ANIMATION_DAMPEN,
	UI_SCALAR_ANIMATION_LENGTH_LIMIT,
	UI_SCALAR_HOVER_LIFT,

    UI_SCALAR_WINDOW_MARGIN,
    UI_SCALAR_WIDGET_MARGIN,
    UI_SCALAR_TEXT_MARGIN,

	UI_SCALAR_ROUNDEDNESS,

    UI_SCALAR_TEXT_ALIGN_X,
    UI_SCALAR_TEXT_ALIGN_Y,

    UI_SCALAR_SCROLLBAR_WIDTH,
    UI_SCALAR_SLIDER_HANDLE_RATIO,

    UI_SCALAR_COUNT,
} ui_style_scalar_t;

typedef enum ui_style_color_t
{
    UI_COLOR_TEXT,
    UI_COLOR_TEXT_SHADOW,

	UI_COLOR_WIDGET_SHADOW,

    UI_COLOR_WINDOW_BACKGROUND,
    UI_COLOR_WINDOW_TITLE_BAR,
    UI_COLOR_WINDOW_TITLE_BAR_HOT,
    UI_COLOR_WINDOW_CLOSE_BUTTON,
    UI_COLOR_WINDOW_OUTLINE,

    UI_COLOR_PROGRESS_BAR_EMPTY,
    UI_COLOR_PROGRESS_BAR_FILLED,

    UI_COLOR_BUTTON_IDLE,
    UI_COLOR_BUTTON_HOT,
    UI_COLOR_BUTTON_ACTIVE,
    UI_COLOR_BUTTON_FIRED,

    UI_COLOR_SLIDER_BACKGROUND,
    UI_COLOR_SLIDER_FOREGROUND,
    UI_COLOR_SLIDER_HOT,
    UI_COLOR_SLIDER_ACTIVE,

    UI_COLOR_COUNT,
} ui_style_color_t;

typedef struct ui_style_t
{
	pool_t animation_state;
	table_t animation_index;

	float base_scalars[UI_SCALAR_COUNT];
	v4_t  base_colors [UI_COLOR_COUNT];

    stack_t(float, UI_STYLE_STACK_COUNT) scalars[UI_SCALAR_COUNT];
    stack_t(v4_t,  UI_STYLE_STACK_COUNT) colors [UI_COLOR_COUNT];

	string_t font_data; // so we can rebuild the font at different sizes without going out to disk
	font_atlas_t font;

	string_t header_font_data; // so we can rebuild the font at different sizes without going out to disk
	font_atlas_t header_font;
} ui_style_t;

fn ui_anim_t *ui_get_anim        (ui_id_t id, v4_t init_value);
fn float ui_interpolate_f32      (ui_id_t id, float target);
fn float ui_interpolate_f32_start(ui_id_t id, float target, float start);
fn float ui_set_f32              (ui_id_t id, float target);
fn v4_t  ui_interpolate_v4       (ui_id_t id, v4_t  target);
fn v4_t  ui_set_v4               (ui_id_t id, v4_t  target);

fn float ui_scalar            (ui_style_scalar_t scalar);
fn void  ui_push_scalar       (ui_style_scalar_t scalar, float value);
fn float ui_pop_scalar        (ui_style_scalar_t scalar);

fn void  ui_push_scalar2      (ui_style_scalar_t scalar, float value);
fn float ui_pop_scalar2       (void);

#define UI_SCALAR(scalar, value) DEFER_LOOP(ui_push_scalar(scalar, value), ui_pop_scalar(scalar))

fn v4_t  ui_color             (ui_style_color_t color);
fn void  ui_push_color        (ui_style_color_t color, v4_t value);
fn v4_t  ui_pop_color         (ui_style_color_t color);

fn void  ui_push_color2       (ui_style_color_t color, v4_t value);
fn v4_t  ui_pop_color2        (void);

#define UI_COLOR(color, value) DEFER_LOOP(ui_push_color(color, value), ui_pop_color(color))

fn void  ui_set_font_height   (float height);
fn float ui_font_height       (void);
fn float ui_header_font_height(void);

//
// Draw
//

typedef enum ui_text_op_t
{
	UI_TEXT_OP_BOUNDS,
	UI_TEXT_OP_DRAW,
	UI_TEXT_OP_COUNT,
} ui_text_op_t;

fn rect2_t ui_text_op                      (font_atlas_t *font, v2_t p, string_t text, v4_t color, ui_text_op_t op);
fn v2_t    ui_text_align_p                 (font_atlas_t *font, rect2_t rect, string_t text, v2_t align);
fn v2_t    ui_text_center_p                (font_atlas_t *font, rect2_t rect, string_t text);

fn void    ui_push_clip_rect               (rect2_t rect);
fn void    ui_pop_clip_rect                (void);

fn rect2_t ui_draw_text                    (font_atlas_t *font, v2_t p, string_t text);
fn rect2_t ui_draw_text_aligned            (font_atlas_t *font, rect2_t rect, string_t text, v2_t align);
fn rect2_t ui_text_bounds                  (font_atlas_t *font, v2_t p, string_t text);
fn float   ui_text_width                   (font_atlas_t *font, string_t text);
fn float   ui_text_height                  (font_atlas_t *font, string_t text);
fn v2_t    ui_text_dim                     (font_atlas_t *font, string_t text);

fn void    ui_draw_rect                    (rect2_t rect, v4_t color);
fn void    ui_draw_rect_shadow             (rect2_t rect, v4_t color, float shadow_amount, float shadow_radius);
fn void    ui_draw_rect_roundedness        (rect2_t rect, v4_t color, v4_t roundness);
fn void    ui_draw_rect_roundedness_shadow (rect2_t rect, v4_t color, v4_t roundness, float shadow_amount, float shadow_radius);
fn void    ui_draw_rect_roundedness_outline(rect2_t rect, v4_t color, v4_t roundedness, float width);
fn void    ui_draw_rect_outline            (rect2_t rect, v4_t color, float outline_width);
fn void    ui_draw_circle                  (v2_t p, float r, v4_t color);

//
// Widget Building Utilities
//

fn bool    ui_mouse_in_rect           (rect2_t rect);
fn float   ui_hover_lift              (ui_id_t id);
fn float   ui_button_style_hover_lift (ui_id_t id);
fn rect2_t ui_cut_widget_rect         (v2_t min_size);
fn float   ui_roundedness_ratio       (rect2_t rect);
fn float   ui_roundedness_ratio_to_abs(rect2_t rect, float ratio);

typedef uint32_t ui_interaction_t;
typedef enum ui_interaction_enum_t
{
	UI_PRESSED  = 0x1,
	UI_HELD     = 0x2,
	UI_RELEASED = 0x4,
	UI_FIRED    = 0x8,
	UI_HOVERED  = 0x10,
	UI_HOT      = 0x20,
} ui_interaction_enum_t;

typedef enum ui_priority_t
{
	UI_PRIORITY_DEFAULT,
	UI_PRIORITY_OVERLAY,
} ui_priority_t;

// I still think this is a little odd
fn ui_interaction_t ui_default_widget_behaviour_priority(ui_id_t id, rect2_t rect, ui_priority_t priority);
fn ui_interaction_t ui_default_widget_behaviour(ui_id_t id, rect2_t rect);

//
// Base Widgets
//

typedef struct ui_text_edit_state_t
{
	size_t selection_start;
	size_t cursor;
} ui_text_edit_state_t;

typedef struct ui_panel_state_t
{
	float scrollable_height_x;
	float scrollable_height_y;
	float scroll_offset_x;
	float scroll_offset_y;
} ui_panel_state_t;

typedef enum ui_panel_flags_enum_t
{
	UI_PANEL_SCROLLABLE_HORZ = 0x1,
	UI_PANEL_SCROLLABLE_VERT = 0x2,
} ui_panel_flags_enum_t;

fn void ui_panel_begin   (rect2_t rect);
fn void ui_panel_begin_ex(ui_id_t id, rect2_t rect, ui_panel_flags_t flags);
fn void ui_panel_end     (void);

#define UI_PANEL(rect) DEFER_LOOP(ui_panel_begin(rect), ui_panel_end())

typedef uint32_t ui_slider_flags_t;
typedef enum ui_slider_flags_enum_t
{
	UI_SLIDER_FLAGS_INC_DEC_BUTTONS = 0x1,
} ui_slider_flags_enum_t;

fn void ui_seperator     (void);
fn void ui_label         (string_t text);
fn void ui_header        (string_t text);
fn void ui_progress_bar  (string_t text, float progress);
fn bool ui_button        (string_t text);
fn bool ui_checkbox      (string_t text, bool *value);
fn bool ui_option_buttons(string_t text, int *value, int count, string_t *names);
fn bool ui_combo_box     (string_t text, size_t *selected_index, size_t count, string_t *names);
fn bool ui_slider        (string_t text, float *value, float min, float max);
fn bool ui_slider_int    (string_t text, int *value, int min, int max);
fn bool ui_slider_int_ex (string_t text, int *value, int min, int max, ui_slider_flags_t flags);
fn void ui_text_edit     (string_t label, dynamic_string_t *buffer);
fn void ui_tooltip       (string_t text);
fn void ui_hover_tooltip (string_t text);

fn bool ui_popup_is_open(ui_id_t id);
fn void ui_open_popup(ui_id_t id);
fn void ui_close_popup(ui_id_t id);

//
//
//

typedef struct ui_state_t
{
	ui_id_t id;

	uint64_t created_frame_index;
	uint64_t last_touched_frame_index;

	bool opened;

	union
	{
		ui_panel_state_t     panel;
		ui_text_edit_state_t text_edit;
	};
} ui_state_t;

#define UI_TOOLTIP_STACK_COUNT (16)
#define UI_MAX_TOOLTIP_LENGTH  (256)

typedef struct ui_tooltip_t
{
	string_storage_t(UI_MAX_TOOLTIP_LENGTH) text;
} ui_tooltip_t;

typedef enum ui_render_layer_t
{
	UI_LAYER_BACKGROUND,
	UI_LAYER_FOREGROUND,
	UI_LAYER_OVERLAY_BACKGROUND,
	UI_LAYER_OVERLAY_FOREGROUND,
	UI_LAYER_TOOLTIP,
} ui_render_layer_t;

typedef struct ui_render_command_key_t
{
	union
	{
		struct
		{
			uint16_t command_index;
			uint8_t  layer;
			uint8_t  window;
		};
		uint32_t u32;
	};
} ui_render_command_key_t;

typedef struct ui_render_command_t
{
    texture_handle_t texture;
    r_ui_rect_t rect;
} ui_render_command_t;

#define UI_CLIP_RECT_STACK_COUNT (32)
#define UI_RENDER_COMMANDS_CAPACITY (8192)

typedef struct ui_render_command_list_t
{
	size_t                   capacity;
	size_t                   count;
	ui_render_command_key_t *keys;
	ui_render_command_t     *commands;
} ui_render_command_list_t;

fn void ui_push_command(ui_render_command_key_t key, const ui_render_command_t *command);
fn void ui_reset_render_commands(void);
fn void ui_sort_render_commands(void);

#define UI_ID_STACK_COUNT (32)

typedef struct ui_popup_t
{
	ui_id_t id;
} ui_popup_t;

#define UI_POPUP_STACK_COUNT (32)

typedef struct debug_notif_t
{
	struct debug_notif_t *next;

	ui_id_t  id;
	v4_t     color;
	float    init_lifetime;
	float    lifetime;
	string_t text;
} debug_notif_t;

typedef struct ui_t
{
	bool initialized;

	arena_t arena;

	arena_t  frame_arenas[2];
	uint64_t frame_index;

	bool has_focus;
	bool hovered;

	hires_time_t current_time;
	hires_time_t hover_time;

	float hover_time_seconds;

	ui_id_t hot;
	ui_id_t next_hot;
	ui_id_t active;

	ui_priority_t override_priority;
	ui_priority_t next_hot_priority;

	ui_id_t next_id;
	rect2_t next_rect;

	ui_id_t next_hovered_panel;
	ui_id_t hovered_panel;

	ui_id_t next_hovered_widget;
	ui_id_t hovered_widget;

	ui_id_t focused_id;

	v2_t    drag_anchor;
	v2_t    drag_offset;
	rect2_t resize_original_rect;

	string_storage_t(UI_MAX_TOOLTIP_LENGTH) next_tooltip;

	stack_t(ui_id_t, UI_ID_STACK_COUNT) id_stack;
	stack_t(ui_tooltip_t, UI_TOOLTIP_STACK_COUNT) tooltip_stack;
	stack_t(ui_popup_t, UI_POPUP_STACK_COUNT) popup_stack;

	pool_t  state;
	table_t state_index;

	ui_input_t   input;
	ui_panels_t  panels;
	ui_windows_t windows;
	ui_style_t   style;

	debug_notif_t *first_debug_notif;
	uint64_t next_notif_id;

	stack_t(r_rect2_fixed_t, UI_CLIP_RECT_STACK_COUNT) clip_rect_stack;
	ui_render_layer_t render_layer;
    ui_render_command_list_t render_commands;

    size_t last_frame_ui_rect_count;
} ui_t;

fn ui_t ui;

fn_local arena_t *ui_frame_arena(void)
{
	arena_t *result = &ui.frame_arenas[ui.frame_index & 1];
	return result;
}

fn bool ui_is_cold         (ui_id_t id);
fn bool ui_is_next_hot     (ui_id_t id);
fn bool ui_is_hot          (ui_id_t id);
fn bool ui_is_active       (ui_id_t id);
fn bool ui_is_hovered_panel(ui_id_t id);

fn void ui_set_next_hot  (ui_id_t id, ui_priority_t priority);
fn void ui_set_hot       (ui_id_t id); // maybe should be never used?
fn void ui_set_active    (ui_id_t id);

fn void ui_clear_next_hot(void);
fn void ui_clear_hot     (void);
fn void ui_clear_active  (void);

fn bool ui_has_focus     (void);
fn bool ui_id_has_focus(ui_id_t id);

fn void ui_hoverable       (ui_id_t id, rect2_t rect);
fn bool ui_is_hovered      (ui_id_t id);
fn bool ui_is_hovered_delay(ui_id_t id, float delay);

fn ui_state_t *ui_get_state(ui_id_t id);
fn bool ui_state_is_new(ui_state_t *state);

fn bool ui_begin(float dt);
fn void ui_end(void);
fn ui_render_command_list_t *ui_get_render_commands(void);

fn void debug_text    (v4_t color, string_t fmt, ...);
fn void debug_text_va (v4_t color, string_t fmt, va_list args);

fn void debug_notif   (v4_t color, float time, string_t fmt, ...);
fn void debug_notif_va(v4_t color, float time, string_t fmt, va_list args);

// internal use
fn void debug_notif_replicate(debug_notif_t *notif);

//
//
//

#endif
