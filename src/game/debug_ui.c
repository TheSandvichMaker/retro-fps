#include "core/string.h"
#include "core/arena.h"
#include "core/math.h"

#include "render/render.h"
#include "render/render_helpers.h"

#include "game/input.h"
#include "game/asset.h"
#include "game/debug_ui.h"

typedef struct ui_id_t
{
	uint64_t value;
} ui_id_t;

DREAM_INLINE ui_id_t ui_id(string_t string)
{
	uint64_t hash = string_hash(string);

	if (hash == 0)
		hash = ~(uint64_t)0;

	ui_id_t result = {
		.value = hash,
	};
	return result;
}

DREAM_INLINE ui_id_t ui_id_pointer(void *pointer)
{
	ui_id_t result = {
		.value = (uintptr_t)pointer,
	};
	return result;
}

typedef struct ui_t
{
	bool initialized;

	uint64_t frame_index;

	float dt;

	ui_id_t hot;
	ui_id_t active;

	ui_style_t style;

	v2_t mouse_p;
	v2_t mouse_pressed_p;
	v2_t mouse_pressed_offset;

	bitmap_font_t font;
} ui_t;

static ui_t ui;

typedef struct ui_widget_t
{
	ui_id_t id;

	uint64_t last_touched_frame_index;

	float t;
} ui_widget_t;

static bulk_t widgets = INIT_BULK_DATA(ui_widget_t);
static hash_t widget_index;

DREAM_INLINE ui_widget_t *ui_get_widget(ui_id_t id)
{
	ui_widget_t *result = hash_find_object(&widget_index, id.value);

	if (!result)
	{
		result = bd_add(&widgets);
		result->id = id;
		hash_add_object(&widget_index, id.value, result);
	}

	ASSERT(result->id.value == id.value);

	result->last_touched_frame_index = ui.frame_index;
	return result;
}

DREAM_INLINE bool ui_is_hot(ui_id_t id)
{
	return ui.hot.value == id.value;
}

DREAM_INLINE bool ui_is_active(ui_id_t id)
{
	return ui.active.value == id.value;
}

DREAM_INLINE void ui_set_hot(ui_id_t id)
{
	if (!ui.active.value)
	{
		ui.hot = id;
	}
}

DREAM_INLINE void ui_set_active(ui_id_t id)
{
	ui.active = id;
}

DREAM_INLINE void ui_clear_hot(void)
{
	ui.hot.value = 0;
}

DREAM_INLINE void ui_clear_active(void)
{
	ui.active.value = 0;
}

void ui_begin(float dt)
{
	if (!ui.initialized)
	{
		image_t *font_image = get_image_from_string(S("gamedata/textures/font.png"));
        ui.font.w       = font_image->w;
        ui.font.h       = font_image->h;
        ui.font.cw      = 10;
        ui.font.ch      = 12;
        ui.font.texture = font_image->gpu;

		ui.initialized = true;

		ui_style_t *style = &ui.style;
		style->text              = make_v4(0.90f, 0.90f, 0.90f, 1.0f);
		style->window.background = make_v4(0.15f, 0.15f, 0.15f, 1.0f);
		style->window.title_bar  = make_v4(0.45f, 0.25f, 0.25f, 1.0f);
		style->button.background = make_v4(0.25f, 0.25f, 0.25f, 1.0f);
		style->button.hot        = make_v4(0.25f, 0.35f, 0.65f, 1.0f);
		style->button.active     = make_v4(0.35f, 0.45f, 0.85f, 1.0f);
		style->button.fired      = make_v4(0.45f, 0.30f, 0.25f, 1.0f);
		style->slider.background = make_v4(0.25f, 0.25f, 0.25f, 1.0f);
		style->slider.foreground = make_v4(0.25f, 0.35f, 0.65f, 1.0f);
		style->slider.hot        = make_v4(0.35f, 0.45f, 0.85f, 1.0f);
		style->slider.active     = make_v4(0.45f, 0.30f, 0.25f, 1.0f);
	}

	ui.frame_index += 1;

	for (bd_iter_t it = bd_iter(&widgets); bd_iter_valid(&it); bd_iter_next(&it))
	{
		ui_widget_t *widget = it.data;

		if (widget->last_touched_frame_index + 1 < ui.frame_index)
		{
			hash_rem(&widget_index, widget->id.value);
			bd_rem_item(&widgets, widget);
		}
	}

	ui_clear_hot();
	ui.dt = dt;

    int mouse_x, mouse_y;
    get_mouse(&mouse_x, &mouse_y);

	ui.mouse_p = (v2_t){ (float)mouse_x, (float)mouse_y };
}

rect2_t ui_window(string_t label, rect2_t rect)
{
	if (NEVER(!ui.initialized)) return (rect2_t){ 0 };

	rect2_t bar = ui_add_top(&rect, (float)ui.font.ch);

	// TODO: This drawing API is horrible fix it. FIX IT. DO SOMETHING ABOUT IT
	// TODO: This drawing API is horrible fix it. FIX IT. DO SOMETHING ABOUT IT
	// TODO: This drawing API is horrible fix it. FIX IT. DO SOMETHING ABOUT IT
	// TODO: This drawing API is horrible fix it. FIX IT. DO SOMETHING ABOUT IT
	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(NULL);
		r_push_rect2_filled(draw, bar, ui.style.window.title_bar);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture, .clip_rect = bar });
		r_push_text(draw, &ui.font, bar.min, ui.style.text, label);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(NULL);
		r_push_rect2_filled(draw, rect, ui.style.window.background);
		r_immediate_draw_end(draw);
	}

	return rect;
}

typedef enum ui_interaction_t
{
	UI_PRESSED  = 0x1,
	UI_HELD     = 0x2,
	UI_RELEASED = 0x4,
} ui_interaction_t;

DREAM_INLINE uint32_t ui_button_behaviour(ui_id_t id, rect2_t rect)
{
	uint32_t result = 0;

	if (ui_is_active(id))
	{
		result |= UI_HELD;

		if (ui_button_released(BUTTON_FIRE1))
		{
			if (rect2_contains_point(rect, ui.mouse_p))
			{
				result |= UI_RELEASED;
			}

			ui_clear_active();
		}
	}
	else if (rect2_contains_point(rect, ui.mouse_p))
	{
		ui_set_hot(id);
	}

	if (ui_is_hot(id) &&
		ui_button_pressed(BUTTON_FIRE1))
	{
		result |= UI_PRESSED;
		ui.mouse_pressed_p      = ui.mouse_p;
		ui.mouse_pressed_offset = sub(ui.mouse_p, rect2_center(rect));
		ui_set_active(id);
	}

	return result;
}

bool ui_button(ui_cut_t cut, string_t label)
{
	if (NEVER(!ui.initialized)) return false;

	bool result = false;

	ui_id_t          id = ui_id(label);
	ui_widget_t *widget = ui_get_widget(id);

	float a = 0.0f;

	switch (cut.side)
	{
		case UI_CUT_LEFT: 
		case UI_CUT_RIGHT:
		{
			a = (float)label.count*ui.font.cw;
		} break;

		case UI_CUT_TOP:
		case UI_CUT_BOTTOM:
		{
			a = (float)ui.font.ch;
		} break;
	}

	rect2_t rect = ui_do_cut(cut, a);

	uint32_t interaction = ui_button_behaviour(id, rect);
	result = interaction & UI_RELEASED;

	if (result)
	{
		widget->t = 1.0f;
	}

	v4_t color = ui.style.button.background;

	if (ui_is_hot(id))
		color = ui.style.button.hot;

	if (ui_is_active(id))
		color = ui.style.button.active;

	if (widget->t >= 0.0f)
	{
		float rate = 0.5f;

		float t = widget->t;
		t *= t;
		t *= t;

		color = v4_lerps(color, ui.style.button.fired, t);

		widget->t -= ui.dt / rate;

		if (widget->t < 0.0f)
			widget->t = 0.0f;
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .clip_rect = rect });
		r_push_rect2_filled(draw, rect, color);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture, .clip_rect = rect });
		r_push_text(draw, &ui.font, rect.min, ui.style.text, label);
		r_immediate_draw_end(draw);
	}

	return result;
}

void ui_slider(rect2_t *layout, string_t label, float *v, float min, float max)
{
	if (NEVER(!v)) return;

	ui_id_t          id = ui_id_pointer(v);
	ui_widget_t *widget = ui_get_widget(id);

	rect2_t outer      = ui_cut_bottom(layout, (float)ui.font.ch);
	rect2_t label_rect = ui_cut_left(&outer, (float)label.count*ui.font.cw);

	float handle_width = 16.0f;
	float handle_half_width = 0.5f*handle_width;
	
	float width = rect2_width(outer);
	float relative_x = CLAMP((ui.mouse_p.x - ui.mouse_pressed_offset.x) - outer.min.x - handle_half_width, 0.0f, width - handle_width);

	float pct = 0.0f;

	if (ui_is_active(id))
	{
		pct = relative_x / (width - handle_width);
		*v = min + pct*(max - min);
	}
	else
	{
		float value = *v;
		pct = (value - min) / (max - min); // TODO: protect against division by zero, think about min > max case
	}

	float width_exclusive = width - handle_width;
	float handle_offset = pct*width_exclusive;

	rect2_t left   = ui_cut_left(&outer, handle_offset);
	rect2_t handle = ui_cut_left(&outer, handle_width);
	rect2_t right  = outer;

	uint32_t interaction = ui_button_behaviour(id, handle);

	if (interaction & UI_RELEASED)
		widget->t = 1.0f;

	v4_t color = ui.style.slider.foreground;

	if (ui_is_hot(id))
		color = ui.style.slider.hot;

	if (ui_is_active(id))
		color = ui.style.slider.active;

	if (widget->t >= 0.0f)
	{
		float rate = 0.5f;

		float t = widget->t;
		t *= t;
		t *= t;

		color = v4_lerps(color, ui.style.slider.active, t);

		widget->t -= ui.dt / rate;

		if (widget->t < 0.0f)
			widget->t = 0.0f;
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(NULL);
		r_push_rect2_filled(draw, left, ui.style.slider.background);
		r_push_rect2_filled(draw, handle, color);
		r_push_rect2_filled(draw, right, ui.style.slider.background);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture, .clip_rect = label_rect });
		r_push_text(draw, &ui.font, label_rect.min, ui.style.text, label);
		r_immediate_draw_end(draw);
	}
}
