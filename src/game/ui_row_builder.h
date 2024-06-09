#pragma once

#define UI_ROW_CHECKBOX_ON_LEFT 1

typedef struct ui_row_builder_t
{
	rect2_t  rect;
	uint32_t row_index;
} ui_row_builder_t;

fn ui_row_builder_t ui_make_row_builder(rect2_t rect);

fn rect2_t ui_row_ex   (ui_row_builder_t *builder, float height, bool draw_background);
fn rect2_t ui_row      (ui_row_builder_t *builder);
fn void    ui_row_split(ui_row_builder_t *builder, rect2_t *label, rect2_t *widget);

fn void ui_row_header       (ui_row_builder_t *builder, string_t label);
fn void ui_row_label        (ui_row_builder_t *builder, string_t label);
fn void ui_row_progress_bar (ui_row_builder_t *builder, string_t label, float progress);
fn bool ui_row_button       (ui_row_builder_t *builder, string_t label);
fn bool ui_row_radio_buttons(ui_row_builder_t *builder, string_t label, int *state, string_t *labels, int count);
fn bool ui_row_checkbox     (ui_row_builder_t *builder, string_t label, bool *v);
fn bool ui_row_slider_int   (ui_row_builder_t *builder, string_t label, int   *v, int   min, int   max);
fn bool ui_row_slider       (ui_row_builder_t *builder, string_t label, float *f, float min, float max);
