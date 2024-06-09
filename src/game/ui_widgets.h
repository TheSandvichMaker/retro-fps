#pragma once

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

typedef struct ui_scrollable_region_state_t
{
	ui_scrollable_region_flags_t flags;
	rect2_t start_rect;
	float   scrolling_zone_x;
	float   scrolling_zone_y;
	float   scroll_offset_x;
	float   scroll_offset_y;
	v2_t    interpolated_scroll_offset;
} ui_scrollable_region_state_t;

fn rect2_t ui_scrollable_region_begin_ex(ui_id_t id, rect2_t start_rect, ui_scrollable_region_flags_t flags);
fn rect2_t ui_scrollable_region_begin   (ui_id_t id, rect2_t start_rect);
fn void    ui_scrollable_region_end     (ui_id_t id, rect2_t final_rect);

//
// Header
//

fn void ui_header_new(rect2_t rect, string_t label);

//
// Label
//

fn void ui_label_new(rect2_t rect, string_t label);

//
// Progress Bar
//

fn void ui_progress_bar_new(rect2_t rect, string_t label, float progress);

//
// Button
//

fn bool ui_button_new(rect2_t rect, string_t label);

//
// Checkbox
//

fn bool ui_checkbox_new(rect2_t rect, bool *result_value);

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

typedef enum ui_slider_flags_new_t
{
	UiSliderFlags_inc_dec_buttons = 0x1,
} ui_slider_flags_new_t;

typedef struct ui_slider_params_t
{
	ui_slider_type_t      type;
	ui_slider_flags_new_t flags;

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

fn void ui_slider_base(ui_id_t id, rect2_t rect, ui_slider_params_t *p);
fn bool ui_slider_new_ex(rect2_t rect, float *v, float min, float max, float granularity, ui_slider_flags_new_t flags);
fn bool ui_slider_new(rect2_t rect, float *v, float min, float max);
fn bool ui_slider_int_new_ex(rect2_t rect, int32_t *v, int32_t min, int32_t max, ui_slider_flags_new_t flags);
fn bool ui_slider_int_new(rect2_t rect, int32_t *v, int32_t min, int32_t max);

//
// Text Edit
//

typedef struct ui_text_edit_state_t
{
	size_t selection_start;
	size_t cursor;
} ui_text_edit_state_t;

fn void ui_text_edit(rect2_t rect, dynamic_string_t *buffer);
