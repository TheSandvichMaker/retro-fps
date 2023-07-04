#ifndef DREAM_DEBUG_UI_H
#define DREAM_DEBUG_UI_H

#include "core/api_types.h"
#include "core/string.h"

typedef struct ui_id_t
{
	uint64_t value;
} ui_id_t;

#define UI_ID_NULL (ui_id_t){0}

DREAM_INLINE ui_id_t ui_child_id(ui_id_t parent, string_t string)
{
	uint64_t hash = string_hash_with_seed(string, parent.value);

	if (hash == 0)
		hash = ~(uint64_t)0;

	ui_id_t result = {
		.value = hash,
	};
	return result;
}

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

typedef enum ui_cut_side_t
{
	UI_CUT_LEFT,
	UI_CUT_RIGHT,
	UI_CUT_TOP,
	UI_CUT_BOTTOM,
	UI_CUT_COUNT,
} ui_cut_side_t;

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

typedef struct ui_style_t
{
	float widget_margin;
	float   text_margin;

	float scrollbar_width;

	v4_t text;

	struct
	{
		v4_t background;
		v4_t title_bar;
	} window;

	struct
	{
		v4_t background;
		v4_t hot;
		v4_t active;
		v4_t fired;
	} button;

	struct
	{
		v4_t background;
		v4_t foreground;
		v4_t hot;
		v4_t active;
	} slider;
} ui_style_t;

typedef uint32_t ui_panel_flags_t;
typedef enum ui_panel_flags_enum_t
{
	UI_PANEL_SCROLLABLE_HORZ = 0x1,
	UI_PANEL_SCROLLABLE_VERT = 0x2,
} ui_panel_flags_enum_t;

DREAM_API void ui_set_style(const ui_style_t *style);

DREAM_API bool ui_begin(float dt);
DREAM_API void ui_end(void);

DREAM_API void ui_window_begin(string_t label, rect2_t size);
DREAM_API void ui_window_end(void);

DREAM_API void ui_panel_begin(rect2_t size);
DREAM_API void ui_panel_begin_ex(ui_id_t id, rect2_t size, ui_panel_flags_t flags);
DREAM_API void ui_panel_end(void);

DREAM_API rect2_t *ui_layout_rect(void);
DREAM_API void ui_set_layout_direction(ui_cut_side_t side);
DREAM_API void ui_set_next_rect(rect2_t rect);
DREAM_API float ui_divide_space(float item_count);

DREAM_API bool ui_button(string_t label);
DREAM_API void ui_slider(string_t label, float *v, float min, float max);

#endif
