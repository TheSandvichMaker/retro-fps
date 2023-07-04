#include "core/string.h"
#include "core/arena.h"
#include "core/math.h"

#include "render/render.h"
#include "render/render_helpers.h"

#include "game/input.h"
#include "game/asset.h"
#include "game/debug_ui.h"

typedef struct ui_panel_t
{
	union
	{
		struct ui_panel_t *parent;
		struct ui_panel_t *next_free;
	};
	ui_id_t          id;
	ui_cut_side_t    layout_direction;
	ui_panel_flags_t flags;
	rect2_t          init_rect;
	rect2_t          rect;
} ui_panel_t;

typedef struct ui_t
{
	bool initialized;

	arena_t arena;

	uint64_t frame_index;

	float dt;

	bool has_focus;
	bool hovered;

	ui_id_t hot;
	ui_id_t active;

	ui_style_t style;

	v2_t mouse_p;
	v2_t mouse_pressed_p;
	v2_t mouse_pressed_offset;

	ui_panel_t *panel;
	ui_panel_t *first_free_panel;

	rect2_t next_rect;

	bitmap_font_t font;
} ui_t;

static ui_t ui;

DREAM_INLINE ui_panel_t *ui_push_panel(rect2_t rect)
{
	if (!ui.first_free_panel)
	{
		ui.first_free_panel = m_alloc_struct_nozero(&ui.arena, ui_panel_t);
		ui.first_free_panel->next_free = NULL;
	}

	ui_panel_t *panel = ui.first_free_panel;
	ui.first_free_panel = panel->next_free;

	zero_struct(panel);

	panel->layout_direction = UI_CUT_TOP;
	panel->rect = rect;

	panel->parent = ui.panel;
	ui.panel = panel;

	return panel;
}

DREAM_INLINE void ui_pop_panel(void)
{
	if (ALWAYS(ui.panel))
	{
		ui_panel_t *panel = ui.panel;
		ui.panel = ui.panel->parent;

		panel->next_free = ui.first_free_panel;
		ui.first_free_panel = panel;
	}
}

DREAM_INLINE ui_panel_t *ui_panel(void)
{
	static ui_panel_t dummy_panel = { 0 };

	ui_panel_t *result = &dummy_panel;

	if (ALWAYS(ui.panel))
	{
		result = ui.panel;
	}

	return result;
}

rect2_t *ui_layout_rect(void)
{
	ui_panel_t *panel = ui_panel();
	return &panel->rect;
}

void ui_set_layout_direction(ui_cut_side_t side)
{
	ui_panel_t *panel = ui_panel();
	panel->layout_direction = side;
}

void ui_set_next_rect(rect2_t rect)
{
	ui.next_rect = rect;
}

float ui_divide_space(float item_count)
{
	float size = 0.0f;

	if (item_count > 0.0001f)
	{
		ui_panel_t *panel = ui_panel();

		switch (panel->layout_direction)
		{
			case UI_CUT_LEFT:
			case UI_CUT_RIGHT:
			{
				size = rect2_width(panel->rect) / item_count;
			} break;

			case UI_CUT_TOP:
			case UI_CUT_BOTTOM:
			{
				size = rect2_height(panel->rect) / item_count;
			} break;
		}
	}

	return size;
}

typedef struct ui_widget_t
{
	ui_id_t id;

	uint64_t last_touched_frame_index;

	float scrollable_height_x;
	float scrollable_height_y;
	float scroll_offset_x;
	float scroll_offset_y;

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

DREAM_INLINE bool ui_override_rect(rect2_t *override)
{
	bool result = false;

	if (rect2_area(ui.next_rect) > 0.0f)
	{
		*override = ui.next_rect;
		ui.next_rect = (rect2_t){ 0 };

		result = true;
	}

	return result;
}

bool ui_begin(float dt)
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
		style->widget_margin       = 1.0f;
		style->text_margin         = 1.0f;
		style->scrollbar_width     = 12.0f;
		style->text                = make_v4(0.90f, 0.90f, 0.90f, 1.0f);
		style->window.background   = make_v4(0.15f, 0.15f, 0.15f, 1.0f);
		style->window.title_bar    = make_v4(0.45f, 0.25f, 0.25f, 1.0f);
		style->progress_bar.empty  = make_v4(0.18f, 0.18f, 0.18f, 1.0f);
		style->progress_bar.filled = make_v4(0.15f, 0.25f, 0.45f, 1.0f);
		style->button.background   = make_v4(0.25f, 0.25f, 0.25f, 1.0f);
		style->button.hot          = make_v4(0.25f, 0.35f, 0.65f, 1.0f);
		style->button.active       = make_v4(0.35f, 0.45f, 0.85f, 1.0f);
		style->button.fired        = make_v4(0.45f, 0.30f, 0.25f, 1.0f);
		style->slider.background   = make_v4(0.25f, 0.25f, 0.25f, 1.0f);
		style->slider.foreground   = make_v4(0.25f, 0.35f, 0.65f, 1.0f);
		style->slider.hot          = make_v4(0.35f, 0.45f, 0.85f, 1.0f);
		style->slider.active       = make_v4(0.45f, 0.30f, 0.25f, 1.0f);
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
	ui.hovered = false;
	ui.dt = dt;

    int mouse_x, mouse_y;
    get_mouse(&mouse_x, &mouse_y);

	ui.mouse_p = (v2_t){ (float)mouse_x, (float)mouse_y };

	return ui.has_focus;
}

void ui_end(void)
{
	if (ui_button_pressed(BUTTON_FIRE1|BUTTON_FIRE2))
	{
		ui.has_focus = ui.hovered;
	}

	ASSERT(ui.panel == NULL);
}

typedef enum ui_interaction_t
{
	UI_PRESSED  = 0x1,
	UI_HELD     = 0x2,
	UI_RELEASED = 0x4,
	UI_FIRED    = 0x8,
} ui_interaction_t;

DREAM_INLINE uint32_t ui_button_behaviour(ui_id_t id, rect2_t rect)
{
	uint32_t result = 0;

	rect2_t hit_rect = rect;

	if (ui.panel)
		hit_rect = rect2_intersect(ui.panel->init_rect, rect);

	if (ui_is_active(id))
	{
		result |= UI_HELD;

		if (ui_button_released(BUTTON_FIRE1))
		{
			if (rect2_contains_point(hit_rect, ui.mouse_p))
			{
				result |= UI_FIRED;
			}

			result |= UI_RELEASED;

			ui_clear_active();
		}
	}
	else if (rect2_contains_point(hit_rect, ui.mouse_p))
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

DREAM_INLINE void ui_check_hovered(rect2_t rect)
{
	if (rect2_contains_point(rect, ui.mouse_p))
	{
		ui.hovered = true;
	}
}

DREAM_INLINE float ui_widget_padding(void)
{
	return 2.0f*ui.style.widget_margin + 2.0f*ui.style.text_margin;
}

void ui_window_begin(string_t label, rect2_t rect)
{
	ASSERT(ui.initialized);

	ui_check_hovered(rect);

	rect2_t bar = ui_add_top(&rect, (float)ui.font.ch + 2.0f*ui.style.text_margin);
	ui_check_hovered(bar);

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(NULL);
		r_push_rect2_filled(draw, bar, ui.style.window.title_bar);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture, .clip_rect = bar });
		r_push_text(draw, &ui.font, add(bar.min, make_v2(ui.style.text_margin, ui.style.text_margin)), ui.style.text, label);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(NULL);
		r_push_rect2_filled(draw, rect, ui.style.window.background);
		r_immediate_draw_end(draw);
	}

	ui_id_t id = ui_id(label);
	ui_panel_begin_ex(id, rect, UI_PANEL_SCROLLABLE_VERT);
}

void ui_window_end(void)
{
	ASSERT(ui.initialized);
	ui_panel_end();
}

void ui_panel_begin(rect2_t rect)
{
	ui_panel_begin_ex(UI_ID_NULL, rect, 0);
}

void ui_panel_begin_ex(ui_id_t id, rect2_t rect, ui_panel_flags_t flags)
{
	ASSERT(ui.initialized);

	r_push_view_screenspace_clip_rect(rect);

	rect2_t inner_rect = ui_shrink(&rect, ui.style.widget_margin);

	float offset_y = 0.0f;

	if (flags & UI_PANEL_SCROLLABLE_VERT)
	{
		rect2_t scroll_area = ui_cut_right(&rect, ui.style.scrollbar_width);
		ui_widget_t *widget = ui_get_widget(id);
		
		if (widget->scrollable_height_y > 0.0f)
		{
			ui_id_t scrollbar_id = ui_child_id(id, S("scrollbar"));

			float scroll_area_height = rect2_height(scroll_area);

			float visible_area_ratio = rect2_height(inner_rect) / widget->scrollable_height_y;
			float handle_size      = visible_area_ratio*scroll_area_height;
			float handle_half_size = 0.5f*handle_size;

			float pct = widget->scroll_offset_y / (widget->scrollable_height_y - rect2_height(inner_rect));

			if (ui_is_active(scrollbar_id))
			{
				float relative_y = CLAMP((ui.mouse_p.y - ui.mouse_pressed_offset.y) - scroll_area.min.y - handle_half_size, 0.0f, scroll_area_height - handle_size);
				pct = 1.0f - (relative_y / (scroll_area_height - handle_size));
				widget->scroll_offset_y = pct*(widget->scrollable_height_y - rect2_height(inner_rect));
			}

			float height_exclusive = scroll_area_height - handle_size;
			float handle_offset = pct*height_exclusive;

			rect2_t top    = ui_cut_top(&scroll_area, handle_offset);
			rect2_t handle = ui_cut_top(&scroll_area, handle_size);
			rect2_t bot    = scroll_area;

			uint32_t interaction = ui_button_behaviour(scrollbar_id, handle);

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
				r_push_rect2_filled(draw, top, ui.style.slider.background);
				r_push_rect2_filled(draw, handle, color);
				r_push_rect2_filled(draw, bot, ui.style.slider.background);
				r_immediate_draw_end(draw);
			}

			offset_y = widget->scroll_offset_y;
		}
	}

	inner_rect = ui_shrink(&rect, ui.style.widget_margin);
	rect2_t init_rect = inner_rect;

	inner_rect = rect2_add_offset(inner_rect, make_v2(0, offset_y));

	if (flags & UI_PANEL_SCROLLABLE_VERT)
	{
		inner_rect.min.y = inner_rect.max.y;
	}

	ui_panel_t *panel = ui_push_panel(inner_rect);
	panel->id        = id;
	panel->flags     = flags;
	panel->init_rect = init_rect;
}

void ui_panel_end(void)
{
	ASSERT(ui.initialized);

	ui_panel_t  *panel  = ui.panel;
	ui_widget_t *widget = ui_get_widget(panel->id);

	if (panel->flags & UI_PANEL_SCROLLABLE_VERT)
	{
		widget->scrollable_height_y = abs_ss(panel->rect.max.y - panel->rect.min.y);
	}

	r_pop_view();
	ui_pop_panel();
}

void ui_label(string_t label)
{
	if (NEVER(!ui.initialized)) 
		return;

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();

		float a = ui_widget_padding();

		switch (panel->layout_direction)
		{
			case UI_CUT_LEFT:
			case UI_CUT_RIGHT:
			{
				a += (float)label.count*ui.font.cw;
			} break;

			case UI_CUT_TOP:
			case UI_CUT_BOTTOM:
			{
				a += (float)ui.font.ch;
			} break;
		}

		rect = ui_do_cut((ui_cut_t){ .side = panel->layout_direction, .rect = &panel->rect }, a);
	}

	rect = ui_shrink(&rect, ui.style.widget_margin);

	rect2_t text_rect = ui_shrink(&rect, ui.style.text_margin);

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture, .clip_rect = text_rect });
		r_push_text(draw, &ui.font, text_rect.min, ui.style.text, label);
		r_immediate_draw_end(draw);
	}
}

void ui_progress_bar(string_t label, float progress)
{
	if (NEVER(!ui.initialized)) 
		return;

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();

		float a = ui_widget_padding();

		switch (panel->layout_direction)
		{
			case UI_CUT_LEFT:
			case UI_CUT_RIGHT:
			{
				a += (float)label.count*ui.font.cw;
			} break;

			case UI_CUT_TOP:
			case UI_CUT_BOTTOM:
			{
				a += (float)ui.font.ch;
			} break;
		}

		rect = ui_do_cut((ui_cut_t){ .side = panel->layout_direction, .rect = &panel->rect }, a);
	}

	rect = ui_shrink(&rect, ui.style.widget_margin);

	rect2_t text_rect = ui_shrink(&rect, ui.style.text_margin);

	// TODO: Unhardcore direction

	float width = rect2_width(rect);

	rect2_t filled = ui_cut_left(&rect, progress*width);
	rect2_t empty  = rect;

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(NULL);
		r_push_rect2_filled(draw, filled, ui.style.progress_bar.filled);
		r_push_rect2_filled(draw, empty,  ui.style.progress_bar.empty);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture });
		r_push_text(draw, &ui.font, text_rect.min, ui.style.text, label);
		r_immediate_draw_end(draw);
	}
}

bool ui_button(string_t label)
{
	bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	ui_id_t          id = ui_id(label);
	ui_widget_t *widget = ui_get_widget(id);

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();

		float a = ui_widget_padding();

		switch (panel->layout_direction)
		{
			case UI_CUT_LEFT:
			case UI_CUT_RIGHT:
			{
				a += (float)label.count*ui.font.cw;
			} break;

			case UI_CUT_TOP:
			case UI_CUT_BOTTOM:
			{
				a += (float)ui.font.ch;
			} break;
		}

		rect = ui_do_cut((ui_cut_t){ .side = panel->layout_direction, .rect = &panel->rect }, a);
	}

	rect = ui_shrink(&rect, ui.style.widget_margin);

	rect2_t text_rect = ui_shrink(&rect, ui.style.text_margin);

	uint32_t interaction = ui_button_behaviour(id, rect);
	result = interaction & UI_FIRED;

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
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture, .clip_rect = text_rect });
		r_push_text(draw, &ui.font, text_rect.min, ui.style.text, label);
		r_immediate_draw_end(draw);
	}

	return result;
}

bool ui_checkbox(string_t label, bool *value)
{
	bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	ui_id_t          id = ui_id(label);
	ui_widget_t *widget = ui_get_widget(id);

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();

		float a = ui_widget_padding();

		switch (panel->layout_direction)
		{
			case UI_CUT_LEFT:
			case UI_CUT_RIGHT:
			{
				a += (float)label.count*ui.font.cw + ui.font.ch;
			} break;

			case UI_CUT_TOP:
			case UI_CUT_BOTTOM:
			{
				a += (float)ui.font.ch;
			} break;
		}

		rect = ui_do_cut((ui_cut_t){ .side = panel->layout_direction, .rect = &panel->rect }, a);
	}

	rect = ui_shrink(&rect, ui.style.widget_margin);

	rect2_t box_rect = ui_cut_left(&rect, (float)ui.font.ch + ui.style.widget_margin);
	rect2_t label_rect = ui_cut_left(&rect, (float)label.count*ui.font.cw);
	label_rect = ui_shrink(&label_rect, ui.style.text_margin);

	uint32_t interaction = ui_button_behaviour(id, box_rect);
	result = interaction & UI_FIRED;

	if (result)
	{
		if (value) *value = !*value;

		widget->t = 1.0f;
	}

	v4_t color = ui.style.button.background;

	if (ui_is_hot(id))
		color = ui.style.button.hot;

	if (ui_is_active(id) || value && *value)
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
		r_immediate_draw_t *draw = r_immediate_draw_begin(NULL);
		r_push_rect2_filled(draw, box_rect, color);
		r_immediate_draw_end(draw);
	}

	{
		r_immediate_draw_t *draw = r_immediate_draw_begin(&(r_immediate_params_t){ .texture = ui.font.texture, });
		r_push_text(draw, &ui.font, label_rect.min, ui.style.text, label);
		r_immediate_draw_end(draw);
	}

	return result;
}

void ui_slider(string_t label, float *v, float min, float max)
{
	if (NEVER(!v)) return;

	ui_id_t          id = ui_id_pointer(v);
	ui_widget_t *widget = ui_get_widget(id);

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = ui_cut_top(&panel->rect, (float)ui.font.ch + ui_widget_padding());
	}

	rect = ui_shrink(&rect, ui.style.widget_margin);

	rect2_t label_rect = ui_cut_left(&rect, (float)label.count*ui.font.cw);
	label_rect = ui_shrink(&label_rect, ui.style.text_margin);

	float handle_width = 16.0f;
	float handle_half_width = 0.5f*handle_width;
	
	float width = rect2_width(rect);
	float relative_x = CLAMP((ui.mouse_p.x - ui.mouse_pressed_offset.x) - rect.min.x - handle_half_width, 0.0f, width - handle_width);

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

	rect2_t left   = ui_cut_left(&rect, handle_offset);
	rect2_t handle = ui_cut_left(&rect, handle_width);
	rect2_t right  = rect;

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
