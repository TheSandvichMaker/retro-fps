#ifndef DREAM_DEBUG_UI_H
#define DREAM_DEBUG_UI_H

#include "core/api_types.h"
#include "core/string.h"
#include "dream/font.h"

typedef struct ui_id_t
{
	uint64_t value;
} ui_id_t;

#define UI_ID_NULL (ui_id_t){0}

typedef enum ui_cut_side_t
{
	UI_CUT_LEFT,
	UI_CUT_RIGHT,
	UI_CUT_TOP,
	UI_CUT_BOTTOM,
	UI_CUT_COUNT,
} ui_cut_side_t;

typedef enum ui_style_scalar_t
{
    UI_SCALAR_ANIMATION_RATE,

	UI_SCALAR_ANIMATION_STIFFNESS,
	UI_SCALAR_ANIMATION_DAMPEN,
	UI_SCALAR_HOVER_LIFT,

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

typedef uint32_t ui_panel_flags_t;
typedef enum ui_panel_flags_enum_t
{
	UI_PANEL_SCROLLABLE_HORZ = 0x1,
	UI_PANEL_SCROLLABLE_VERT = 0x2,
} ui_panel_flags_enum_t;

typedef struct ui_window_t
{
	string_t name;
	ui_id_t  id;

	rect2_t rect;

	bool  open;
	float openness;
} ui_window_t;

DREAM_LOCAL bool ui_begin(float dt);
DREAM_LOCAL void ui_end(void);

DREAM_LOCAL void ui_window_begin(ui_window_t *window);
DREAM_LOCAL void ui_window_end(ui_window_t *window);

#define UI_WINDOW(window) DEFER_LOOP(ui_window_begin(window), ui_window_end(window))

DREAM_LOCAL void ui_panel_begin(rect2_t size);
DREAM_LOCAL void ui_panel_begin_ex(ui_id_t id, rect2_t size, ui_panel_flags_t flags);
DREAM_LOCAL void ui_panel_end(void);

#define UI_PANEL(size) DEFER_LOOP(ui_panel_begin(size), ui_panel_end())

DREAM_LOCAL rect2_t *ui_layout_rect(void);
DREAM_LOCAL void ui_set_layout_direction(ui_cut_side_t side);
DREAM_LOCAL void ui_set_next_rect(rect2_t rect);
DREAM_LOCAL float ui_divide_space(float item_count);

DREAM_LOCAL void ui_set_next_id(ui_id_t id);

DREAM_LOCAL float ui_scalar(ui_style_scalar_t scalar);
DREAM_LOCAL void ui_push_scalar(ui_style_scalar_t scalar, float value);
DREAM_LOCAL float ui_pop_scalar(ui_style_scalar_t scalar);
DREAM_LOCAL v4_t ui_color(ui_style_color_t color);
DREAM_LOCAL void ui_push_color(ui_style_color_t color, v4_t value);
DREAM_LOCAL v4_t ui_pop_color(ui_style_color_t color);

#define UI_SCALAR(scalar, value) DEFER_LOOP(ui_push_scalar(scalar, value), ui_pop_scalar(scalar))
#define UI_COLOR(color, value)   DEFER_LOOP(ui_push_color (color,  value), ui_pop_color (color))

DREAM_LOCAL void ui_label(string_t label);
DREAM_LOCAL float ui_label_width(string_t label);
DREAM_LOCAL void ui_progress_bar(string_t label, float progress);
DREAM_LOCAL bool ui_button(string_t label);
DREAM_LOCAL bool ui_checkbox(string_t label, bool *value);
DREAM_LOCAL bool ui_radio(string_t label, int *value, int count, string_t *labels);
DREAM_LOCAL void ui_slider(string_t label, float *v, float min, float max);
DREAM_LOCAL bool ui_slider_int(string_t label, int *v, int min, int max);

DREAM_LOCAL float ui_interpolate_f32(ui_id_t id, float target);
DREAM_LOCAL float ui_set_f32        (ui_id_t id, float target);

DREAM_LOCAL v4_t ui_interpolate_v4  (ui_id_t id, v4_t target);
DREAM_LOCAL v4_t ui_set_v4          (ui_id_t id, v4_t target);

//
// "Internal"
//

DREAM_INLINE rect2_t ui_cut_left(rect2_t *rect, float a)
{
	float min_x = rect->x0;
	rect->min.x = rect->x0 + a;
	return (rect2_t){ .min = { min_x, rect->min.y }, .max = { rect->min.x, rect->max.y } };
}

DREAM_INLINE rect2_t ui_cut_right(rect2_t *rect, float a)
{
	float max_x = rect->x1;
	rect->max.x = rect->x1 - a;
	return (rect2_t){ .min = { rect->max.x, rect->min.y }, .max = { max_x, rect->max.y } };
}

DREAM_INLINE rect2_t ui_cut_top(rect2_t *rect, float a)
{
	float max_y = rect->y1;
	rect->max.y = rect->y1 - a;
	return (rect2_t){ .min = { rect->min.x, rect->max.y }, .max = { rect->max.x, max_y } };
}

DREAM_INLINE rect2_t ui_cut_bottom(rect2_t *rect, float a)
{
	float min_y = rect->y0;
	rect->min.y = rect->y0 + a;
	return (rect2_t){ .min = { rect->min.x, min_y }, .max = { rect->max.x, rect->min.y } };
}

DREAM_INLINE rect2_t ui_add_left(rect2_t *rect, float a)
{
	float min_x = MIN(rect->x0, rect->x0 - a);
	return (rect2_t){ .min = { min_x, rect->min.y }, .max = { rect->min.x, rect->max.y } };
}

DREAM_INLINE rect2_t ui_add_right(rect2_t *rect, float a)
{
	float max_x = MAX(rect->x0, rect->x0 + a);
	return (rect2_t){ .min = { rect->max.x, rect->min.y }, .max = { max_x, rect->max.y } };
}

DREAM_INLINE rect2_t ui_add_top(rect2_t *rect, float a)
{
	float max_y = MAX(rect->y1, rect->y1 + a);
	return (rect2_t){ .min = { rect->min.x, rect->max.y }, .max = { rect->max.x, max_y } };
}

DREAM_INLINE rect2_t ui_add_bottom(rect2_t *rect, float a)
{
	float min_y = MIN(rect->y0, rect->y0 - a);
	return (rect2_t){ .min = { rect->min.x, min_y }, .max = { rect->max.x, rect->min.y } };
}

DREAM_INLINE rect2_t ui_extend(rect2_t *rect, float a)
{
	rect2_t result = {
		.min = sub(rect->min, a),
		.max = add(rect->max, a),
	};
	return result;
}

DREAM_INLINE rect2_t ui_shrink(rect2_t *rect, float a)
{
	return ui_extend(rect, -a);
}

DREAM_INLINE rect2_t ui_extend2(rect2_t *rect, float x, float y)
{
	v2_t xy = { x, y };

	rect2_t result = {
		.min = sub(rect->min, xy),
		.max = add(rect->max, xy),
	};
	return result;
}

DREAM_INLINE rect2_t ui_shrink2(rect2_t *rect, float x, float y)
{
	return ui_extend2(rect, -x, -y);
}

typedef struct ui_cut_t
{
	rect2_t      *rect;
	ui_cut_side_t side;
} ui_cut_t;

DREAM_INLINE ui_cut_t ui_cut(rect2_t *rect, ui_cut_side_t side)
{
	return (ui_cut_t){
		.rect = rect,
		.side = side,
	};
}

DREAM_INLINE rect2_t ui_do_cut(ui_cut_t cut, float a)
{
	switch (cut.side)
	{
		case UI_CUT_LEFT:   return ui_cut_left  (cut.rect, a);
		case UI_CUT_RIGHT:  return ui_cut_right (cut.rect, a);
		case UI_CUT_TOP:    return ui_cut_top   (cut.rect, a);
		case UI_CUT_BOTTOM: return ui_cut_bottom(cut.rect, a);
		INVALID_DEFAULT_CASE;
	}

	return (rect2_t){ 0 };
}

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

#define UI_STYLE_STACK_COUNT 32

typedef struct ui_t
{
	bool initialized;

	arena_t arena;

	uint64_t frame_index;

	float dt;

	bool has_focus;
	bool hovered;

	ui_id_t hot;
	ui_id_t next_hot;
	ui_id_t active;

	float base_scalars[UI_SCALAR_COUNT];
	v4_t  base_colors [UI_COLOR_COUNT];

    stack_t(float, UI_STYLE_STACK_COUNT) scalars[UI_SCALAR_COUNT];
    stack_t(v4_t,  UI_STYLE_STACK_COUNT) colors [UI_COLOR_COUNT];

	v2_t mouse_p;
	v2_t mouse_pressed_p;
	v2_t drag_anchor;

	ui_panel_t *panel;
	ui_panel_t *first_free_panel;

	ui_id_t next_id;
	rect2_t next_rect;

	// bitmap_font_t font;
	font_atlas_t font;
} ui_t;

DREAM_LOCAL ui_t ui;

DREAM_LOCAL void ui_set_font_size(float size);

DREAM_LOCAL bool ui_is_cold(ui_id_t id);
DREAM_LOCAL bool ui_is_hot(ui_id_t id);
DREAM_LOCAL bool ui_is_active(ui_id_t id);
DREAM_LOCAL void ui_set_hot(ui_id_t id);
DREAM_LOCAL void ui_set_active(ui_id_t id);
DREAM_LOCAL void ui_clear_hot(void);
DREAM_LOCAL void ui_clear_active(void);

typedef struct ui_widget_t
{
	ui_id_t id;

	uint64_t created_frame_index;
	uint64_t last_touched_frame_index;

    rect2_t rect;
	float scrollable_height_x;
	float scrollable_height_y;
	float scroll_offset_x;
	float scroll_offset_y;

	v4_t interp_color;
} ui_widget_t;

DREAM_LOCAL ui_widget_t *ui_get_widget(ui_id_t id);
DREAM_LOCAL bool ui_widget_is_new(ui_widget_t *widget);

DREAM_LOCAL float ui_widget_padding(void);
DREAM_LOCAL bool ui_override_rect(rect2_t *override);
DREAM_LOCAL rect2_t ui_default_label_rect(string_t label);

DREAM_INLINE float ui_animate_towards(float at, float target)
{
    float rate = ui_scalar(UI_SCALAR_ANIMATION_RATE);

	float t = rate*ui.dt;
	float result = lerp(target, at, saturate(t));
	return result;
}

DREAM_INLINE v4_t ui_animate_towards_exp4(v4_t at, v4_t target)
{
    float rate = ui_scalar(UI_SCALAR_ANIMATION_RATE);

	float t = rate*ui.dt;
	v4_t result = v4_lerps(target, at, saturate(t));
	return result;
}

DREAM_INLINE ui_id_t ui_child_id(ui_id_t parent, string_t string)
{
	ui_id_t result = {0};

	if (ui.next_id.value)
	{
		result = ui.next_id;
		ui.next_id.value = 0;
	}
	else
	{
		uint64_t hash = string_hash_with_seed(string, parent.value);

		if (hash == 0)
			hash = ~(uint64_t)0;

		result.value = hash;
	}

	return result;
}

DREAM_INLINE ui_id_t ui_id(string_t string)
{
	return ui_child_id(UI_ID_NULL, string);
}

DREAM_INLINE ui_id_t ui_id_pointer(void *pointer)
{
	ui_id_t result = {
		.value = (uintptr_t)pointer,
	};
	return result;
}

#endif
