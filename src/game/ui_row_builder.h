#pragma once

typedef uint32_t ui_row_builder_flags_t;
typedef enum ui_row_builder_flags_enum_t
{
	UiRowBuilder_insert_row_separators = 0x1,
	UiRowBuilder_checkbox_on_left      = 0x2,
} ui_row_builder_flags_enum_t;

typedef struct ui_row_builder_t
{
	rect2_t  rect;
	rect2_t  covered_rect;
	rect2_t  last_row;
	uint32_t row_index;
	ui_row_builder_flags_t flags;
} ui_row_builder_t;

typedef struct ui_row_builder_params_t
{
	ui_row_builder_flags_t flags;
} ui_row_builder_params_t;

fn ui_row_builder_t ui_make_row_builder_ex(rect2_t rect, const ui_row_builder_params_t *params);
fn ui_row_builder_t ui_make_row_builder   (rect2_t rect);

fn rect2_t ui_row_ex       (ui_row_builder_t *builder, float height, bool draw_background);
fn rect2_t ui_row          (ui_row_builder_t *builder);
fn void    ui_row_split    (ui_row_builder_t *builder, rect2_t *label, rect2_t *widget);
fn void    ui_row_spacer   (ui_row_builder_t *builder, ui_size_t size);
fn void    ui_row_separator(ui_row_builder_t *builder);

fn void ui_row_error_widget      (ui_row_builder_t *builder, string_t widget_name, string_t error_message);
fn bool ui_row_collapsable_region(ui_row_builder_t *builder, string_t label, bool *open);
fn void ui_row_header            (ui_row_builder_t *builder, string_t label);
fn void ui_row_label             (ui_row_builder_t *builder, string_t label);
fn void ui_row_labels2           (ui_row_builder_t *builder, string_t k, string_t v);
fn void ui_row_progress_bar      (ui_row_builder_t *builder, string_t label, float progress);
fn bool ui_row_button            (ui_row_builder_t *builder, string_t label);
fn bool ui_row_radio_buttons     (ui_row_builder_t *builder, string_t label, int *state, string_t *labels, int count);
fn bool ui_row_checkbox          (ui_row_builder_t *builder, string_t label, bool *v);
fn bool ui_row_slider_int        (ui_row_builder_t *builder, string_t label, int   *v, int   min, int   max);
fn bool ui_row_slider            (ui_row_builder_t *builder, string_t label, float *f, float min, float max);
fn bool ui_row_slider_ex         (ui_row_builder_t *builder, string_t label, float *f, float min, float max, float granularity);
fn void ui_row_text_edit         (ui_row_builder_t *builder, string_t label, dynamic_string_t *buffer);
fn void ui_row_text_edit_ex      (ui_row_builder_t *builder, string_t label, dynamic_string_t *buffer, const ui_text_edit_params_t *params);

// TODO: Pull out as generic widget not tied to row builder

typedef struct ui_color_picker_state_t
{
	bool popup_open;
	v2_t popup_position;

	v4_t color_before_editing;

	// want to preserve this rather than reconstruct it from the color
	// because if you put the value to 0, then all hue and saturation
	// info is destroyed (also just seems gnarly precision wise)
	float hue, sat, val;
	// but we also want to know if the color changed externally, so we
	// aren't holding stale hue/sat/val values!
	v4_t  cached_color;
} ui_color_picker_state_t;

fn void ui_row_color_picker   (ui_row_builder_t *builder, string_t label, v4_t *color);
fn void ui_row_color_picker_v3(ui_row_builder_t *builder, string_t label, v3_t *color);
