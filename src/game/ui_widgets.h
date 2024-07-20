#pragma once

//
// Fallback "Error" Widget
//

fn void ui_error_widget(rect2_t rect, string_t widget_name, string_t error_message);

//
// Scrollable Region
//

typedef enum ui_scrollable_region_flags_t
{
	UiScrollableRegionFlags_scroll_horizontal = 0x1,
	UiScrollableRegionFlags_scroll_vertical   = 0x2,
	UiScrollableRegionFlags_scroll_both       = UiScrollableRegionFlags_scroll_horizontal|UiScrollableRegionFlags_scroll_vertical,
	UiScrollableRegionFlags_draw_scroll_bar   = 0x4,
	UiScrollableRegionFlags_always_draw_horizontal_scroll_bar = 0x10,
	UiScrollableRegionFlags_always_draw_vertical_scroll_bar   = 0x20,
} ui_scrollable_region_flags_t;

typedef struct ui_scrollable_region_t
{
	ui_scrollable_region_flags_t flags;
	rect2_t start_rect;
	v2_t    scroll_zone;
	v2_t    scroll_offset;
	size_t  render_commands_start_index;
} ui_scrollable_region_t;

fn rect2_t ui_scrollable_region_begin_ex(ui_scrollable_region_t *state, rect2_t start_rect, ui_scrollable_region_flags_t flags);
fn rect2_t ui_scrollable_region_begin   (ui_scrollable_region_t *state, rect2_t start_rect);
fn void    ui_scrollable_region_end     (ui_scrollable_region_t *state, rect2_t final_rect);

//
// Collapsable Region
//

fn bool ui_collapsable_region(rect2_t rect, string_t title, bool *open);

//
// Header
//

fn void ui_header(rect2_t rect, string_t label);

//
// Label
//

fn void ui_label(rect2_t rect, string_t label);

//
// Progress Bar
//

fn void ui_progress_bar(rect2_t rect, string_t label, float progress);

//
// Button
//

fn bool ui_button(rect2_t rect, string_t label);

//
// Checkbox
//

fn bool ui_checkbox(rect2_t rect, bool *result_value);

//
// "Radio Buttons" (not really)
//

fn bool ui_radio_buttons(rect2_t rect, int *state, string_t *labels, int count);

//
// Slider
//

typedef enum ui_slider_type_t
{
	UiSlider_none,

	UiSlider_f32,
	UiSlider_i32,

	UiSlider_COUNT,
} ui_slider_type_t;

typedef enum ui_slider_flags_t
{
	UiSliderFlags_inc_dec_buttons = 0x1,
} ui_slider_flags_t;

typedef struct ui_slider_parameters_t
{
	ui_id_t id;
	ui_slider_type_t type;

	ui_slider_flags_t flags;

	double min;
	double max;
	double granularity;
	double increment_amount;
	double major_increment_amount;

	union
	{
		void    *v;
		float   *f32;
		int32_t *i32;
	};

	const char *format_string;
} ui_slider_parameters_t;

fn bool ui_slider_base(rect2_t rect, const ui_slider_parameters_t *p);
fn bool ui_slider_ex  (rect2_t rect, float   *v, float   min, float   max, float granularity);
fn bool ui_slider     (rect2_t rect, float   *v, float   min, float   max);
fn bool ui_slider_int (rect2_t rect, int32_t *v, int32_t min, int32_t max);

fn bool ui_drag_float (rect2_t rect, float   *v);
fn bool ui_draw_int   (rect2_t rect, int32_t *v);

//
// Text Edit
//

typedef struct ui_text_edit_state_t
{
	bool actively_editing;

	int selection_start;
	int cursor;

	bool                  clear;
	string_storage_t(256) storage;
	dynamic_string_t      string;
} ui_text_edit_state_t;

typedef struct ui_text_edit_params_t
{
	bool auto_storage       : 1;
	bool clear_after_commit : 1;
	bool numeric_only       : 1;

	string_t preview_text;
	float    align_x;
} ui_text_edit_params_t;

typedef uint32_t ui_text_edit_result_t;
typedef enum ui_text_edit_result_enum_t
{
	UiTextEditResult_edited     = 0x1,
	UiTextEditResult_committed  = 0x2,
	UiTextEditResult_terminated = 0x4,
} ui_text_edit_result_enum_t;

fn ui_text_edit_result_t ui_text_edit_ex(rect2_t rect, dynamic_string_t *buffer, const ui_text_edit_params_t *params);
fn ui_text_edit_result_t ui_text_edit(rect2_t rect, dynamic_string_t *buffer);

//
// Color Picker
//

fn void ui_hue_picker    (rect2_t rect, float *hue);
fn void ui_sat_val_picker(rect2_t rect, float hue, float *sat, float *val);
fn void ui_color_picker  (rect2_t rect, v4_t *color);

//
// Window
//

typedef struct ui_window_t
{
	rect2_t rect;

	bool open;
	bool focused;
} ui_window_t;

fn bool ui_window_begin(ui_id_t id, ui_window_t *window, string_t title);
fn void ui_window_end  (ui_id_t id, ui_window_t *window);
