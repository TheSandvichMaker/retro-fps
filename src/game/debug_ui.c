#include "core/string.h"
#include "core/arena.h"
#include "core/math.h"

#include "render/render.h"
#include "render/render_helpers.h"

#include "game/input.h"
#include "game/asset.h"
#include "game/debug_ui.h"

ui_t ui;

float ui_scalar(ui_style_scalar_t scalar)
{
    return stack_top(ui.style.scalars[scalar]);
}

void ui_push_scalar(ui_style_scalar_t scalar, float value)
{
    stack_push(ui.style.scalars[scalar], value);
}

float ui_pop_scalar(ui_style_scalar_t scalar)
{
    return stack_pop(ui.style.scalars[scalar]);
}

v4_t ui_color(ui_style_color_t color)
{
    return stack_top(ui.style.colors[color]);
}

void ui_push_color(ui_style_color_t color, v4_t value)
{
    stack_push(ui.style.colors[color], value);
}

v4_t ui_pop_color(ui_style_color_t color)
{
    return stack_pop(ui.style.colors[color]);
}

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
    else
    {
        zero_struct(result);
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

void ui_set_next_id(ui_id_t id)
{
	ui.next_id = id;
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

static bulk_t widgets = INIT_BULK_DATA(ui_widget_t);
static hash_t widget_index;

ui_widget_t *ui_get_widget(ui_id_t id)
{
	ui_widget_t *result = hash_find_object(&widget_index, id.value);

	if (!result)
	{
		result = bd_add(&widgets);
		result->id = id;
		result->new = true;
		hash_add_object(&widget_index, id.value, result);
	}

	ASSERT(result->id.value == id.value);

	result->last_touched_frame_index = ui.frame_index;
	return result;
}

bool ui_is_cold(ui_id_t id)
{
	return (ui.hot.value != id.value &&
			ui.active.value != id.value);
}

bool ui_is_hot(ui_id_t id)
{
	return ui.hot.value == id.value;
}

bool ui_is_active(ui_id_t id)
{
	return ui.active.value == id.value;
}

void ui_set_hot(ui_id_t id)
{
	if (!ui.active.value)
	{
		ui.hot = id;
	}
}

void ui_set_next_hot(ui_id_t id)
{
    ui.next_hot = id;
}

void ui_set_active(ui_id_t id)
{
	ui.active = id;
}

void ui_clear_hot(void)
{
	ui.hot.value = 0;
}

void ui_clear_active(void)
{
	ui.active.value = 0;
}

float ui_widget_padding(void)
{
	return 2.0f*ui_scalar(UI_SCALAR_WIDGET_MARGIN) + 2.0f*ui_scalar(UI_SCALAR_TEXT_MARGIN);
}

bool ui_override_rect(rect2_t *override)
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

rect2_t ui_default_label_rect(string_t label)
{
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
	return rect;
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

        ui_push_scalar(UI_SCALAR_ANIMATION_RATE      , 40.0f);
		ui_push_scalar(UI_SCALAR_WIDGET_MARGIN       , 2.0f);
		ui_push_scalar(UI_SCALAR_TEXT_MARGIN         , 4.0f);
		ui_push_scalar(UI_SCALAR_TEXT_ALIGN_X        , 0.5f);
		ui_push_scalar(UI_SCALAR_TEXT_ALIGN_Y        , 0.5f);
		ui_push_scalar(UI_SCALAR_SCROLLBAR_WIDTH     , 12.0f);
        ui_push_scalar(UI_SCALAR_SLIDER_HANDLE_RATIO , 3.0f);
		ui_push_color (UI_COLOR_TEXT                 , make_v4(0.90f, 0.90f, 0.90f, 1.0f));
		ui_push_color (UI_COLOR_TEXT_SHADOW          , make_v4(0.00f, 0.00f, 0.00f, 0.75f));
		ui_push_color (UI_COLOR_WINDOW_BACKGROUND    , make_v4(0.15f, 0.15f, 0.15f, 1.0f));
		ui_push_color (UI_COLOR_WINDOW_TITLE_BAR     , make_v4(0.45f, 0.25f, 0.25f, 1.0f));
		ui_push_color (UI_COLOR_WINDOW_CLOSE_BUTTON  , make_v4(0.35f, 0.15f, 0.15f, 1.0f));
		ui_push_color (UI_COLOR_PROGRESS_BAR_EMPTY   , make_v4(0.18f, 0.18f, 0.18f, 1.0f));
		ui_push_color (UI_COLOR_PROGRESS_BAR_FILLED  , make_v4(0.15f, 0.25f, 0.45f, 1.0f));
		ui_push_color (UI_COLOR_BUTTON_IDLE          , make_v4(0.28f, 0.28f, 0.28f, 1.0f));
		ui_push_color (UI_COLOR_BUTTON_HOT           , make_v4(0.25f, 0.35f, 0.65f, 1.0f));
		ui_push_color (UI_COLOR_BUTTON_ACTIVE        , make_v4(0.35f, 0.45f, 0.85f, 1.0f));
		ui_push_color (UI_COLOR_BUTTON_FIRED         , make_v4(0.45f, 0.30f, 0.25f, 1.0f));
		ui_push_color (UI_COLOR_SLIDER_BACKGROUND    , make_v4(0.18f, 0.18f, 0.18f, 1.0f));
		ui_push_color (UI_COLOR_SLIDER_FOREGROUND    , make_v4(0.28f, 0.28f, 0.28f, 1.0f));
		ui_push_color (UI_COLOR_SLIDER_HOT           , make_v4(0.25f, 0.35f, 0.65f, 1.0f));
		ui_push_color (UI_COLOR_SLIDER_ACTIVE        , make_v4(0.35f, 0.45f, 0.85f, 1.0f));
	}

	ui.frame_index += 1;

	for (bd_iter_t it = bd_iter(&widgets); bd_iter_valid(&it); bd_iter_next(&it))
	{
		ui_widget_t *widget = it.data;

		widget->new = false;

		if (widget->last_touched_frame_index + 1 < ui.frame_index)
		{
			hash_rem(&widget_index, widget->id.value);
			bd_rem_item(&widgets, widget);
		}
	}

    if (!ui.active.value)
        ui.hot = ui.next_hot;

	ui.next_hot = UI_ID_NULL;

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
        ui_set_next_hot(id);
	}

	if (ui_is_hot(id) &&
		ui_button_pressed(BUTTON_FIRE1))
	{
		result |= UI_PRESSED;
		ui.mouse_pressed_p = ui.mouse_p;
		ui.drag_anchor     = sub(ui.mouse_p, rect2_center(rect));
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

DREAM_INLINE void ui_text(v2_t p, string_t text)
{
    r_draw_text(&ui.font, add(p, make_v2(1.0f, -1.0f)), ui_color(UI_COLOR_TEXT_SHADOW), text);
    r_draw_text(&ui.font, p, ui_color(UI_COLOR_TEXT), text);
}

DREAM_INLINE v2_t ui_text_center_p(rect2_t rect, string_t text)
{
    float text_width  = (float)text.count*(float)ui.font.cw;
    float text_height = (float)ui.font.ch;

    float rect_width  = rect2_width(rect);
    float rect_height = rect2_height(rect);

    v2_t result = {
        .x = rect.min.x + 0.5f*rect_width  - 0.5f*text_width,
        .y = rect.min.y + 0.5f*rect_height - 0.5f*text_height,
    };

    return result;
}

DREAM_INLINE v2_t ui_text_align_p(rect2_t rect, string_t text)
{
    float align_x = ui_scalar(UI_SCALAR_TEXT_ALIGN_X);
    float align_y = ui_scalar(UI_SCALAR_TEXT_ALIGN_Y);

    float text_width  = (float)text.count*(float)ui.font.cw;
    float text_height = (float)ui.font.ch;

    float rect_width  = rect2_width(rect);
    float rect_height = rect2_height(rect);

    v2_t result = {
        .x = rect.min.x + align_x*rect_width  - align_x*text_width,
        .y = rect.min.y + align_y*rect_height - align_y*text_height,
    };

    return result;
}

void ui_window_begin(string_t label, rect2_t rect, bool *open)
{
	ASSERT(ui.initialized);

    ui_id_t      id     = ui_id(label);
    ui_widget_t *widget = ui_get_widget(id);

    if (widget->new)
    {
        widget->rect = rect;
    }

    v2_t dim = rect2_dim(rect);

    rect = rect2_from_min_dim(widget->rect.min, dim);// goofy 
    ui_check_hovered(rect);

	rect2_t bar = ui_add_top(&rect, (float)ui.font.ch + 2.0f*ui_scalar(UI_SCALAR_TEXT_MARGIN));
	ui_check_hovered(bar);

    rect2_t both = rect2_union(bar, rect);
    uint32_t interaction = ui_button_behaviour(id, both);

    if (interaction & UI_PRESSED)
    {
        ui.drag_anchor = sub(rect.min, ui.mouse_p);
    }

    if (ui_is_active(id))
    {
        widget->rect = rect2_reposition_min(widget->rect, add(ui.mouse_p, ui.drag_anchor));
    }

    // NOTE: the panel will push a very similar view right away but we need it for the rendering here
	r_push_view_screenspace_clip_rect(both);

	r_immediate_rect2_filled(bar, ui_color(UI_COLOR_WINDOW_TITLE_BAR));
	r_immediate_rect2_filled(rect, ui_color(UI_COLOR_WINDOW_BACKGROUND));
	r_immediate_flush();

    if (open)
    {
        ui_set_next_rect(ui_cut_right(&bar, ui_label_width(S("Close"))));

        UI_SCALAR(UI_SCALAR_WIDGET_MARGIN, 0.0f)
        UI_COLOR(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_WINDOW_CLOSE_BUTTON))
        if (ui_button(S("Close")))
        {
            *open = false;
        }
    }

    ui_id_t panel_id = ui_child_id(id, S("panel"));
	ui_text(add(bar.min, make_v2(ui_scalar(UI_SCALAR_TEXT_MARGIN), ui_scalar(UI_SCALAR_TEXT_MARGIN))), label);
	ui_panel_begin_ex(panel_id, rect, UI_PANEL_SCROLLABLE_VERT);
}

void ui_window_end(void)
{
	ASSERT(ui.initialized);

    r_pop_view();
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

	rect2_t inner_rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	float offset_y = 0.0f;

	if (flags & UI_PANEL_SCROLLABLE_VERT)
	{
		rect2_t scroll_area = ui_cut_right(&rect, ui_scalar(UI_SCALAR_SCROLLBAR_WIDTH));
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
				float relative_y = CLAMP((ui.mouse_p.y - ui.drag_anchor.y) - scroll_area.min.y - handle_half_size, 0.0f, scroll_area_height - handle_size);
				pct = 1.0f - (relative_y / (scroll_area_height - handle_size));
				widget->scroll_offset_y = pct*(widget->scrollable_height_y - rect2_height(inner_rect));
			}

			float height_exclusive = scroll_area_height - handle_size;
			float handle_offset = pct*height_exclusive;

			rect2_t top    = ui_cut_top(&scroll_area, handle_offset);
			rect2_t handle = ui_cut_top(&scroll_area, handle_size);
			rect2_t bot    = scroll_area;

			ui_button_behaviour(scrollbar_id, handle);

			v4_t color = ui_color(UI_COLOR_SLIDER_FOREGROUND);

			r_immediate_rect2_filled(top, ui_color(UI_COLOR_SLIDER_BACKGROUND));
			r_immediate_rect2_filled(handle, color);
			r_immediate_rect2_filled(bot, ui_color(UI_COLOR_SLIDER_BACKGROUND));
			r_immediate_flush();

			offset_y = widget->scroll_offset_y;
		}
	}

	inner_rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));
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

	rect2_t rect = ui_default_label_rect(label);
	rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));
	ui_text(ui_text_align_p(text_rect, label), label);
}

float ui_label_width(string_t label)
{
    return (float)label.count*(float)ui.font.cw + ui_widget_padding();
}

void ui_progress_bar(string_t label, float progress)
{
	if (NEVER(!ui.initialized)) 
		return;

	rect2_t rect = ui_default_label_rect(label);
	rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	// TODO: Unhardcore direction

	float width = rect2_width(rect);

	rect2_t filled = ui_cut_left(&rect, progress*width);
	rect2_t empty  = rect;

	r_immediate_rect2_filled(filled, ui_color(UI_COLOR_PROGRESS_BAR_FILLED));
	r_immediate_rect2_filled(empty,  ui_color(UI_COLOR_PROGRESS_BAR_EMPTY));
	r_immediate_flush();

	ui_text(ui_text_align_p(text_rect, label), label);
}

typedef enum ui_widget_state_t
{
	UI_WIDGET_STATE_COLD,
	UI_WIDGET_STATE_HOT,
	UI_WIDGET_STATE_ACTIVE,
} ui_widget_state_t;

DREAM_INLINE ui_widget_state_t ui_get_widget_state(ui_id_t id)
{
	ui_widget_state_t result = UI_WIDGET_STATE_COLD;

	if (ui_is_active(id))
		result = UI_WIDGET_STATE_ACTIVE;
    else if (ui_is_hot(id))
		result = UI_WIDGET_STATE_HOT;

	return result;
}

DREAM_INLINE v4_t ui_animate_colors(ui_id_t id, ui_widget_state_t state, float rate, v4_t cold, v4_t hot, v4_t active, v4_t fired)
{
	(void)fired;

	ui_widget_t *widget = ui_get_widget(id);

	if (widget->new)
	{
		widget->interp_color = cold;
	}

	v4_t target = cold;
	switch (state)
	{
		case UI_WIDGET_STATE_COLD:   target = cold;   break;
		case UI_WIDGET_STATE_HOT:    target = hot;    break;
		case UI_WIDGET_STATE_ACTIVE: target = active; break;
	}

	if (state == UI_WIDGET_STATE_ACTIVE)
		rate *= 0.5f;

	widget->interp_color = ui_animate_towards_exp4(widget->interp_color, target);
	return widget->interp_color;
}

bool ui_button(string_t label)
{
	bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	ui_id_t id = ui_id(label);

	rect2_t rect = ui_default_label_rect(label);
	rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	uint32_t interaction = ui_button_behaviour(id, rect);
	result = interaction & UI_FIRED;

	ui_widget_state_t state = ui_get_widget_state(id);

	v4_t color = ui_animate_colors(id, state, ui_scalar(UI_SCALAR_ANIMATION_RATE),
								   ui_color(UI_COLOR_BUTTON_IDLE),
								   ui_color(UI_COLOR_BUTTON_HOT),
								   ui_color(UI_COLOR_BUTTON_ACTIVE),
								   ui_color(UI_COLOR_BUTTON_FIRED));

	r_immediate_rect2_filled(rect, color);
	r_immediate_flush();

	ui_text(ui_text_center_p(text_rect, label), label);

	return result;
}

bool ui_checkbox(string_t label, bool *value)
{
	bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	if (NEVER(!value)) 
		return result;

	ui_id_t id = ui_id(label);

	rect2_t rect = ui_default_label_rect(label);
	rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t box_rect = ui_cut_left(&rect, rect2_height(rect));
	rect2_t label_rect = ui_cut_left(&rect, (float)label.count*ui.font.cw);
	label_rect = ui_shrink(&label_rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	uint32_t interaction = ui_button_behaviour(id, box_rect);
	result = interaction & UI_FIRED;

	if (result)
	{
		*value = !*value;
	}

	ui_widget_state_t state = ui_get_widget_state(id);

	if (*value && state == UI_WIDGET_STATE_COLD)
		state = UI_WIDGET_STATE_ACTIVE;

	v4_t color = ui_animate_colors(id, state, ui_scalar(UI_SCALAR_ANIMATION_RATE),
								   ui_color(UI_COLOR_BUTTON_IDLE),
								   ui_color(UI_COLOR_BUTTON_HOT),
								   ui_color(UI_COLOR_BUTTON_ACTIVE),
								   ui_color(UI_COLOR_BUTTON_FIRED));

	r_immediate_rect2_filled(box_rect, color);
	r_immediate_flush();

	ui_text(label_rect.min, label);

	return result;
}

bool ui_radio(string_t label, int *value, int count, string_t *labels)
{
    bool result = false;

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = ui_cut_top(&panel->rect, (float)ui.font.ch + ui_widget_padding());
	}

	rect2_t label_rect = ui_cut_left(&rect, (float)label.count*ui.font.cw + ui_widget_padding());
	label_rect = ui_shrink(&label_rect, 0.5f*ui_widget_padding());

    {
        float size = rect2_width(rect) / (float)count;
        for (int i = 0; i < count; i++)
        {
            bool selected = *value == i;

            if (selected)
            {
                ui_push_color(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_BUTTON_ACTIVE));
            }

            ui_set_next_rect(ui_cut_left(&rect, size));
            bool pressed = ui_button(labels[i]);
            result |= pressed;

            if (pressed)
            {
                *value = i;
            }

            if (selected)
            {
                ui_pop_color(UI_COLOR_BUTTON_IDLE);
            }
        }
    }

	ui_text(label_rect.min, label);

    return result;
}

void ui_slider(string_t label, float *v, float min, float max)
{
	if (NEVER(!v)) return;

	ui_id_t id = ui_id_pointer(v);

	// TODO: Handle layout direction properly
	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = ui_cut_top(&panel->rect, (float)ui.font.ch + ui_widget_padding());
	}

	rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	float w = rect2_width(rect);

	rect2_t label_rect = ui_cut_left(&rect, 0.5f*w); // (float)label.count*ui.font.cw + 2.0f*ui_scalar(UI_SCALAR_TEXT_MARGIN));
	label_rect = ui_shrink(&label_rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	rect2_t slider_body = rect;

	float handle_width = rect2_height(rect)*ui_scalar(UI_SCALAR_SLIDER_HANDLE_RATIO);
	float handle_half_width = 0.5f*handle_width;
	
	float width = rect2_width(rect);
	float relative_x = CLAMP((ui.mouse_p.x - ui.drag_anchor.x) - rect.min.x - handle_half_width, 0.0f, width - handle_width);

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

	ui_button_behaviour(id, handle);

	ui_widget_state_t state = ui_get_widget_state(id);

	v4_t color = ui_animate_colors(id, state, ui_scalar(UI_SCALAR_ANIMATION_RATE),
								   ui_color(UI_COLOR_SLIDER_FOREGROUND),
								   ui_color(UI_COLOR_SLIDER_HOT),
								   ui_color(UI_COLOR_SLIDER_ACTIVE),
								   ui_color(UI_COLOR_SLIDER_ACTIVE));

	r_immediate_rect2_filled(left, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	r_immediate_rect2_filled(handle, color);
	r_immediate_rect2_filled(right, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	r_immediate_flush();

	ui_text(label_rect.min, label);

	string_t value_text = Sf("%.03f", *v);

	v2_t text_p = ui_text_center_p(slider_body, value_text);
	ui_text(text_p, value_text);
}

bool ui_slider_int(string_t label, int *v, int min, int max)
{
	if (NEVER(!v)) return false;

	int init_value = *v;

	ui_id_t id = ui_id_pointer(v);

	// TODO: Handle layout direction properly
	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = ui_cut_top(&panel->rect, (float)ui.font.ch + ui_widget_padding());
	}

	float w = rect2_width(rect);

	rect2_t label_rect = ui_cut_left(&rect, 0.5f*w); // (float)label.count*ui.font.cw + ui_widget_padding());
	label_rect = ui_shrink(&label_rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN) + ui_scalar(UI_SCALAR_TEXT_MARGIN));

	rect2_t sub_rect = ui_cut_left (&rect, (float)ui.font.cw + ui_widget_padding());
	rect2_t add_rect = ui_cut_right(&rect, (float)ui.font.cw + ui_widget_padding());

	rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	ui_id_t sub_id = ui_child_id(id, S("sub"));
	ui_set_next_id  (sub_id);
	ui_set_next_rect(sub_rect);
	if (ui_button(S("-")))
	{
		if (*v > min) *v = *v - 1;
	}

	ui_id_t add_id = ui_child_id(id, S("add"));
	ui_set_next_id  (add_id);
	ui_set_next_rect(add_rect);
	if (ui_button(S("+")))
	{
		if (*v < max) *v = *v + 1;
	}

	rect2_t slider_body = rect;

	float handle_width = rect2_height(rect)*ui_scalar(UI_SCALAR_SLIDER_HANDLE_RATIO);
	float handle_half_width = 0.5f*handle_width;
	
	float width = rect2_width(rect);
	float relative_x = CLAMP((ui.mouse_p.x - ui.drag_anchor.x) - rect.min.x - handle_half_width, 0.0f, width - handle_width);

	float pct = 0.0f;

	if (ui_is_active(id))
	{
		pct = relative_x / (width - handle_width);
		*v = (int)roundf((float)min + pct*(float)(max - min));
	}
	else
	{
		int value = *v;
		pct = (float)(value - min) / (float)(max - min); // TODO: protect against division by zero, think about min > max case
	}

	float width_exclusive = width - handle_width;
	float handle_offset = pct*width_exclusive;

	rect2_t left   = ui_cut_left(&rect, handle_offset);
	rect2_t handle = ui_cut_left(&rect, handle_width);
	rect2_t right  = rect;

	ui_button_behaviour(id, handle);

	ui_widget_state_t state = ui_get_widget_state(id);

	v4_t color = ui_animate_colors(id, state, ui_scalar(UI_SCALAR_ANIMATION_RATE),
								   ui_color(UI_COLOR_SLIDER_FOREGROUND),
								   ui_color(UI_COLOR_SLIDER_HOT),
								   ui_color(UI_COLOR_SLIDER_ACTIVE),
								   ui_color(UI_COLOR_SLIDER_ACTIVE));

	r_immediate_rect2_filled(left, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	r_immediate_rect2_filled(handle, color);
	r_immediate_rect2_filled(right, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	r_immediate_flush();

	ui_text(label_rect.min, label);

	string_t value_text = Sf("%d", *v);

	v2_t text_p = ui_text_center_p(slider_body, value_text);
	ui_text(text_p, value_text);

	return *v != init_value;
}
