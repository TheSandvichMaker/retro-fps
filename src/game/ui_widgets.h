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
} ui_scrollable_region_flags_t;

typedef struct ui_scrollable_region_t
{
	ui_scrollable_region_flags_t flags;
	rect2_t start_rect;
	v2_t    interpolated_scroll_offset;
	v2_t    scroll_zone;
	v2_t    scroll_offset;
} ui_scrollable_region_t;

fn rect2_t ui_scrollable_region_begin_ex(ui_scrollable_region_t *state, rect2_t start_rect, ui_scrollable_region_flags_t flags);
fn rect2_t ui_scrollable_region_begin   (ui_scrollable_region_t *state, rect2_t start_rect);
fn void    ui_scrollable_region_end     (ui_scrollable_region_t *state, rect2_t final_rect);

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

typedef struct ui_slider_params_t
{
	ui_slider_type_t      type;
	ui_slider_flags_t flags;

	float increment_amount;

	union
	{
		struct
		{
			float granularity;
			float *v;
			float min;
			float max;
		} f32;

		struct
		{
			int32_t *v;
			int32_t min;
			int32_t max;
		} i32;
	};
} ui_slider_params_t;

fn void ui_slider_base  (ui_id_t id, rect2_t rect, ui_slider_params_t *p);
fn bool ui_slider_ex    (rect2_t rect, float *v, float min, float max, float granularity, ui_slider_flags_t flags);
fn bool ui_slider       (rect2_t rect, float *v, float min, float max);
fn bool ui_slider_int_ex(rect2_t rect, int32_t *v, int32_t min, int32_t max, ui_slider_flags_t flags);
fn bool ui_slider_int   (rect2_t rect, int32_t *v, int32_t min, int32_t max);

//
// Text Edit
//

typedef struct ui_text_edit_state_t
{
	int selection_start;
	int cursor;
} ui_text_edit_state_t;

typedef struct ui_text_edit_params_t
{
	string_t preview_text;
} ui_text_edit_params_t;

typedef uint32_t ui_text_edit_result_t;
typedef enum ui_text_edit_result_enum_t
{
	UiTextEditResult_edited   = 0x1,
	UiTextEditResult_committed = 0x2,
} ui_text_edit_result_enum_t;

fn ui_text_edit_result_t ui_text_edit_ex(rect2_t rect, dynamic_string_t *buffer, const ui_text_edit_params_t *params);
fn ui_text_edit_result_t ui_text_edit(rect2_t rect, dynamic_string_t *buffer);

//
// Color Picker
//

fn void ui_hue_picker    (rect2_t rect, float *hue);
fn void ui_sat_val_picker(rect2_t rect, float hue, float *sat, float *val);
fn void ui_color_picker  (rect2_t rect, v4_t *color);
