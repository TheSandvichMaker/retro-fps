#ifndef DREAM_UI_H
#define DREAM_UI_H

//
// IDs
//

typedef struct ui_id_t
{
	uint64_t value;
} ui_id_t;

#define UI_ID_NULL (ui_id_t){0}

DREAM_INLINE ui_id_t ui_id        (string_t string);
DREAM_INLINE ui_id_t ui_child_id  (ui_id_t parent, string_t string);
DREAM_INLINE ui_id_t ui_id_pointer(void *pointer);

DREAM_INLINE void    ui_set_next_id(ui_id_t id);
DREAM_INLINE void    ui_push_id    (ui_id_t id);
DREAM_INLINE void    ui_pop_id     (void);

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

DREAM_LOCAL void ui_submit_mouse_buttons(platform_mouse_buttons_t buttons, bool pressed);
DREAM_LOCAL void ui_submit_mouse_wheel  (float wheel_delta);
DREAM_LOCAL void ui_submit_mouse_p      (v2_t p);
DREAM_LOCAL void ui_submit_mouse_dp     (v2_t dp);
DREAM_LOCAL void ui_submit_text         (string_t text);

DREAM_LOCAL bool ui_mouse_buttons_down    (platform_mouse_buttons_t buttons);
DREAM_LOCAL bool ui_mouse_buttons_pressed (platform_mouse_buttons_t buttons);
DREAM_LOCAL bool ui_mouse_buttons_released(platform_mouse_buttons_t buttons);

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

DREAM_LOCAL ui_panel_t *ui_push_panel(ui_id_t id, rect2_t rect, ui_panel_flags_t flags);
DREAM_LOCAL void        ui_pop_panel (void);
DREAM_LOCAL ui_panel_t *ui_panel     (void);

DREAM_LOCAL rect2_t *ui_layout_rect         (void);
DREAM_LOCAL void     ui_set_layout_direction(rect2_cut_side_t side);
DREAM_LOCAL void     ui_set_next_rect       (rect2_t rect);
DREAM_LOCAL float    ui_divide_space        (float item_count);

// layout helpers
DREAM_LOCAL float    ui_widget_padding      (void);
DREAM_LOCAL bool     ui_override_rect       (rect2_t *rect);
DREAM_LOCAL rect2_t  ui_default_label_rect  (font_atlas_t *font, string_t label);

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

typedef void (*ui_window_proc_t)(void *userdata);

typedef struct ui_window_t
{
	struct ui_window_t *next;
	struct ui_window_t *prev;

	string_storage_t(256) title;
	rect2_t rect;

	bool open;

	void            *user_data;
	ui_window_proc_t draw_proc;
} ui_window_t;

typedef struct ui_windows_t
{
	ui_window_t *first_window;
	ui_window_t *last_window;
	ui_window_t *focus_window;
} ui_windows_t;

DREAM_LOCAL void ui_add_window            (ui_window_t *window);
DREAM_LOCAL void ui_remove_window         (ui_window_t *window);
DREAM_LOCAL void ui_bring_window_to_front (ui_window_t *window);
DREAM_LOCAL void ui_send_window_to_back   (ui_window_t *window);
DREAM_LOCAL void ui_focus_window          (ui_window_t *window);
DREAM_LOCAL void ui_open_window           (ui_window_t *window);
DREAM_LOCAL void ui_close_window          (ui_window_t *window);
DREAM_LOCAL void ui_toggle_window_openness(ui_window_t *window);

DREAM_LOCAL void ui_process_windows(void);

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
	hash_t animation_index;

	float base_scalars[UI_SCALAR_COUNT];
	v4_t  base_colors [UI_COLOR_COUNT];

    stack_t(float, UI_STYLE_STACK_COUNT) scalars[UI_SCALAR_COUNT];
    stack_t(v4_t,  UI_STYLE_STACK_COUNT) colors [UI_COLOR_COUNT];

	string_t font_data; // so we can rebuild the font at different sizes without going out to disk
	font_atlas_t font;

	string_t header_font_data; // so we can rebuild the font at different sizes without going out to disk
	font_atlas_t header_font;
} ui_style_t;

DREAM_LOCAL ui_anim_t *ui_get_anim     (ui_id_t id, v4_t init_value);
DREAM_LOCAL float ui_interpolate_f32   (ui_id_t id, float target);
DREAM_LOCAL float ui_set_f32           (ui_id_t id, float target);
DREAM_LOCAL v4_t  ui_interpolate_v4    (ui_id_t id, v4_t  target);
DREAM_LOCAL v4_t  ui_set_v4            (ui_id_t id, v4_t  target);

DREAM_LOCAL float ui_scalar            (ui_style_scalar_t scalar);
DREAM_LOCAL void  ui_push_scalar       (ui_style_scalar_t scalar, float value);
DREAM_LOCAL float ui_pop_scalar        (ui_style_scalar_t scalar);

#define UI_SCALAR(scalar, value) DEFER_LOOP(ui_push_scalar(scalar, value), ui_pop_scalar(scalar))

DREAM_LOCAL v4_t  ui_color             (ui_style_color_t color);
DREAM_LOCAL void  ui_push_color        (ui_style_color_t color, v4_t value);
DREAM_LOCAL v4_t  ui_pop_color         (ui_style_color_t color);

#define UI_COLOR(color, value) DEFER_LOOP(ui_push_color(color, value), ui_pop_color(color))

DREAM_LOCAL void  ui_set_font_height   (float height);
DREAM_LOCAL float ui_font_height       (void);
DREAM_LOCAL float ui_header_font_height(void);

//
// Draw
//

typedef enum ui_text_op_t
{
	UI_TEXT_OP_BOUNDS,
	UI_TEXT_OP_DRAW,
	UI_TEXT_OP_COUNT,
} ui_text_op_t;

DREAM_LOCAL rect2_t ui_text_op                      (font_atlas_t *font, v2_t p, string_t text, v4_t color, ui_text_op_t op);
DREAM_LOCAL rect2_t ui_draw_text                    (font_atlas_t *font, v2_t p, string_t text);
DREAM_LOCAL rect2_t ui_draw_text_aligned            (font_atlas_t *font, rect2_t rect, string_t text, v2_t align);
DREAM_LOCAL rect2_t ui_text_bounds                  (font_atlas_t *font, v2_t p, string_t text);
DREAM_LOCAL float   ui_text_width                   (font_atlas_t *font, string_t text);
DREAM_LOCAL float   ui_text_height                  (font_atlas_t *font, string_t text);
DREAM_LOCAL v2_t    ui_text_dim                     (font_atlas_t *font, string_t text);

DREAM_LOCAL void    ui_draw_rect                    (rect2_t rect, v4_t color);
DREAM_LOCAL void    ui_draw_rect_roundedness        (rect2_t rect, v4_t color, v4_t roundness);
DREAM_LOCAL void    ui_draw_rect_roundedness_shadow (rect2_t rect, v4_t color, v4_t roundness, float shadow_amount, float shadow_radius);
DREAM_LOCAL void    ui_draw_rect_roundedness_outline(rect2_t rect, v4_t color, v4_t roundedness, float width);
DREAM_LOCAL void    ui_draw_rect_outline            (rect2_t rect, v4_t color, float outline_width);
DREAM_LOCAL void    ui_draw_circle                  (v2_t p, float r, v4_t color);

//
// Widget Building Utilities
//

DREAM_LOCAL bool ui_hover_rect(rect2_t rect);

typedef uint32_t ui_interaction_t;
typedef enum ui_interaction_enum_t
{
	UI_PRESSED  = 0x1,
	UI_HELD     = 0x2,
	UI_RELEASED = 0x4,
	UI_FIRED    = 0x8,
	UI_HOVERED  = 0x10,
} ui_interaction_enum_t;

DREAM_LOCAL ui_interaction_t ui_widget_behaviour(ui_id_t id, rect2_t rect);

DREAM_LOCAL v2_t ui_text_align_p (font_atlas_t *font, rect2_t rect, string_t text, v2_t align);
DREAM_LOCAL v2_t ui_text_center_p(font_atlas_t *font, rect2_t rect, string_t text);

//
// Base Widgets
//

typedef struct ui_text_edit_state_t
{
	int selection_start;
	int selection_end;
	int cursor;
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

DREAM_LOCAL void ui_panel_begin   (rect2_t rect);
DREAM_LOCAL void ui_panel_begin_ex(ui_id_t id, rect2_t rect, ui_panel_flags_t flags);
DREAM_LOCAL void ui_panel_end     (void);

#define UI_PANEL(rect) DEFER_LOOP(ui_panel_begin(rect), ui_panel_end())

DREAM_LOCAL void ui_seperator     (void);
DREAM_LOCAL void ui_label         (string_t text);
DREAM_LOCAL void ui_header        (string_t text);
DREAM_LOCAL void ui_progress_bar  (string_t text, float progress);
DREAM_LOCAL bool ui_button        (string_t text);
DREAM_LOCAL bool ui_checkbox      (string_t text, bool *value);
DREAM_LOCAL bool ui_option_buttons(string_t text, int *value, int count, string_t *names);
DREAM_LOCAL bool ui_slider        (string_t text, float *value, float min, float max);
DREAM_LOCAL bool ui_slider_int    (string_t text, int *value, int min, int max);
DREAM_LOCAL void ui_text_edit     (string_t label, dynamic_string_t *buffer);

//
//
//

typedef struct ui_state_t
{
	ui_id_t id;

	uint64_t created_frame_index;
	uint64_t last_touched_frame_index;

	union
	{
		ui_panel_state_t     panel;
		ui_text_edit_state_t text_edit;
	};
} ui_state_t;

#define UI_ID_STACK_COUNT (32)

typedef struct ui_t
{
	bool initialized;

	arena_t arena;

	uint64_t frame_index;

	bool has_focus;
	bool hovered;

	ui_id_t hot;
	ui_id_t next_hot;
	ui_id_t active;

	ui_id_t last_id;
	ui_id_t next_id;
	rect2_t next_rect;

	ui_id_t next_hovered_panel;
	ui_id_t hovered_panel;

	ui_id_t focused_id;

	v2_t    drag_anchor;
	v2_t    drag_offset;
	rect2_t resize_original_rect;

	stack_t(ui_id_t, UI_ID_STACK_COUNT) id_stack;

	pool_t state;
	hash_t state_index;

	ui_input_t   input;
	ui_panels_t  panels;
	ui_windows_t windows;
	ui_style_t   style;
} ui_t;

DREAM_LOCAL ui_t ui;

DREAM_LOCAL bool ui_is_cold         (ui_id_t id);
DREAM_LOCAL bool ui_is_next_hot     (ui_id_t id);
DREAM_LOCAL bool ui_is_hot          (ui_id_t id);
DREAM_LOCAL bool ui_is_active       (ui_id_t id);
DREAM_LOCAL bool ui_is_hovered_panel(ui_id_t id);

DREAM_LOCAL void ui_set_next_hot  (ui_id_t id);
DREAM_LOCAL void ui_set_hot       (ui_id_t id);
DREAM_LOCAL void ui_set_active    (ui_id_t id);

DREAM_LOCAL void ui_clear_next_hot(void);
DREAM_LOCAL void ui_clear_hot     (void);
DREAM_LOCAL void ui_clear_active  (void);

DREAM_LOCAL bool ui_has_focus     (void);
DREAM_LOCAL bool ui_id_has_focus(ui_id_t id);

DREAM_LOCAL ui_state_t *ui_get_state(ui_id_t id);
DREAM_LOCAL bool ui_state_is_new(ui_state_t *state);

DREAM_LOCAL bool ui_begin(float dt);
DREAM_LOCAL void ui_end(void);

//
//
//

#endif
