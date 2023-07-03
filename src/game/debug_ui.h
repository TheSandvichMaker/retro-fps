#ifndef DREAM_DEBUG_UI_H
#define DREAM_DEBUG_UI_H

#include "core/api_types.h"

DREAM_INLINE rect2_t ui_cut_left(rect2_t *rect, float a)
{
	float min_x = rect->x0;
	rect->min.x = MIN(rect->x1, rect->x0 + a);
	return (rect2_t){ .min = { min_x, rect->min.y }, .max = { rect->min.x, rect->max.y } };
}

DREAM_INLINE rect2_t ui_cut_right(rect2_t *rect, float a)
{
	float max_x = rect->x1;
	rect->max.x = MAX(rect->x0, rect->x1 - a);
	return (rect2_t){ .min = { rect->max.x, rect->min.y }, .max = { max_x, rect->max.y } };
}

DREAM_INLINE rect2_t ui_cut_top(rect2_t *rect, float a)
{
	float min_y = rect->y0;
	rect->min.y = MIN(rect->y1, rect->y0 + a);
	return (rect2_t){ .min = { rect->min.x, min_y }, .max = { rect->max.x, rect->min.y } };
}

DREAM_INLINE rect2_t ui_cut_bottom(rect2_t *rect, float a)
{
	float max_y = rect->y1;
	rect->max.y = MAX(rect->y0, rect->y1 - a);
	return (rect2_t){ .min = { rect->min.x, rect->max.y }, .max = { rect->max.x, max_y } };
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
	float min_y = MIN(rect->y0, rect->y0 - a);
	return (rect2_t){ .min = { rect->min.x, min_y }, .max = { rect->max.x, rect->min.y } };
}

DREAM_INLINE rect2_t ui_add_bottom(rect2_t *rect, float a)
{
	float max_y = MAX(rect->y0, rect->y0 + a);
	return (rect2_t){ .min = { rect->min.x, rect->max.y }, .max = { rect->max.x, max_y } };
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
} ui_style_t;

DREAM_API void ui_set_style(const ui_style_t *style);

DREAM_API void ui_begin(float dt);
DREAM_API rect2_t ui_window(string_t label, rect2_t size);

DREAM_API bool ui_button(ui_cut_t cut, string_t label);

#endif
