// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct ui_widget_context_t
{
	void *userdata;
	v2_t  min_size;
} ui_widget_context_t;

typedef enum ui_widget_mode_t
{
	UiWidgetMode_none,
	UiWidgetMode_get_size,
	UiWidgetMode_interact_and_draw,
	UiWidgetMode_COUNT,
} ui_widget_mode_t;

typedef void (*ui_widget_func_t)(ui_widget_context_t *context, ui_widget_mode_t mode, rect2_t rect);

typedef enum ui_layout_flow_t
{
	Flow_none,
	Flow_north,
	Flow_east,
	Flow_south,
	Flow_west,
	Flow_COUNT,
} ui_layout_flow_t;

typedef enum ui_axis_t
{
	Axis_x,
	Axis_y,
	Axis_COUNT,
} ui_axis_t;

typedef enum ui_size_kind_t
{
	UiSize_none,
	UiSize_pct,
	UiSize_pix,
	UiSize_COUNT,
} ui_size_kind_t;

typedef struct ui_size_t
{
	ui_size_kind_t kind;
	float          value;
} ui_size_t;

fn_local ui_size_t ui_sz_none()
{
	return (ui_size_t){0};
};

fn_local ui_size_t ui_sz_pct(float pct)
{
	return (ui_size_t){
		.kind = UiSize_pct,
		.value = pct,
	};
}

fn_local ui_size_t ui_sz_pix(float pix)
{
	return (ui_size_t){
		.kind = UiSize_pix,
		.value = pix,
	};
}

typedef struct ui_prepared_rect_t ui_prepared_rect_t;

typedef struct ui_layout_t
{
	rect2_t rect;

	ui_layout_flow_t flow;
	ui_axis_t        flow_axis;

	size_t prepared_rect_count;
	ui_prepared_rect_t *first_prepared_rect;
	ui_prepared_rect_t * last_prepared_rect;

	bool  wants_justify;
	float justify_x;
	float justify_y;
} ui_layout_t;

fn ui_layout_t ui_make_layout(rect2_t starting_rect);

fn void ui_equip_layout  (ui_layout_t *layout);
fn void ui_unequip_layout(void);

fn ui_layout_t *ui_get_layout(void);

fn rect2_t layout_rect(void);

fn void layout_prepare_even_spacing(uint32_t item_count);
fn rect2_t layout_place_widget(v2_t widget_size);

fn rect2_t layout_make_justified_rect(v2_t bounds);

typedef struct ui_layout_justify_t
{
	float x;
	float y;
} ui_layout_justify_t;

fn void layout_begin_justify(const ui_layout_justify_t *justify);
fn void layout_end_justify  (void);

#define Layout_Justify(...)                          \
	DEFER_LOOP(                                      \
		layout_begin_justify(&(ui_layout_justify_t){ \
			.x = 0.0f,                               \
			.y = 0.0f,                               \
		}),                                          \
		layout_end_justify()                         \
	)

fn ui_layout_flow_t layout_begin_flow(ui_layout_flow_t flow);
fn void             layout_end_flow  (ui_layout_flow_t old_flow);

#define Layout_Flow(flow)                                      \
	for (ui_layout_flow_t _old_flow = layout_begin_flow(flow); \
		 _old_flow != Flow_COUNT;                              \
		 layout_end_flow(_old_flow), _old_flow = Flow_COUNT)

typedef struct ui_layout_cut_t
{
	ui_size_t        size;
	bool             push_clip_rect;
	ui_layout_flow_t flow;
	ui_size_t        margin_x;
	ui_size_t        margin_y;
} ui_layout_cut_t;

typedef struct ui_layout_cut_restore_t
{
	bool             exit;
	rect2_t          rect;
	bool             pushed_clip_rect;
	ui_layout_flow_t flow;
} ui_layout_cut_restore_t;

fn ui_layout_cut_restore_t layout_begin_cut_internal(const ui_layout_cut_t *cut);
fn void                    layout_end_cut_internal  (ui_layout_cut_restore_t *restore);

#define Layout_Cut(...)                                                                       \
	for (ui_layout_cut_restore_t _cut_restore = layout_begin_cut_internal(&(ui_layout_cut_t){ \
			 .size           = ui_sz_none(),                                                  \
			 .push_clip_rect = false,                                                         \
			 .flow           = Flow_none,                                                     \
			 .margin_x       = ui_sz_pix(0.0f),                                               \
			 .margin_y       = ui_sz_pix(0.0f),                                               \
			 ##__VA_ARGS__                                                                    \
		 });                                                                                  \
		 !_cut_restore.exit;                                                                  \
		 layout_end_cut_internal(&_cut_restore))


//
// rect-cut redo
//

fn_local float ui_size_to_width(rect2_t rect, ui_size_t size)
{
	float x = 0.0f;

	switch (size.kind)
	{
		case UiSize_none: /* all good! don't do anything! */ break;
		case UiSize_pct: x = size.value*rect2_width(rect);   break;
		case UiSize_pix: x = size.value;                     break;

		default:
	    {
			x = -1.0f;
			log(UI, Error, "Invalid kind of size (kind = %d, value = %f) passed to ui_size_to_width", size.kind, size.value);
	    } break;
	}

	return x;
}

fn_local float ui_size_to_height(rect2_t rect, ui_size_t size)
{
	float y = 0.0f;

	switch (size.kind)
	{
		case UiSize_none: /* all good! don't do anything! */ break;
		case UiSize_pct: y = size.value*rect2_height(rect);  break;
		case UiSize_pix: y = size.value;                     break;

		default:
	    {
			y = -1.0f;
			log(UI, Error, "Invalid kind of size (kind = %d, value = %f) passed to ui_size_to_height", size.kind, size.value);
	    } break;
	}

	return y;
}

fn_local void rect2_cut_from_left(rect2_t rect, ui_size_t size, rect2_t *l, rect2_t *r)
{
	float x = ui_size_to_width(rect, size);

	if (l)
	{
		*l = (rect2_t){
			.min.x = rect.min.x,
			.max.x = rect.min.x + x,
			.min.y = rect.min.y,
			.max.y = rect.max.y,
		};
	}

	if (r)
	{
		*r = (rect2_t){
			.min.x = rect.min.x + x,
			.max.x = rect.max.x,
			.min.y = rect.min.y,
			.max.y = rect.max.y,
		};
	}
}

fn_local void rect2_cut_from_right(rect2_t rect, ui_size_t size, rect2_t *r, rect2_t *l)
{
	float x = ui_size_to_width(rect, size);

	if (l)
	{
		*l = (rect2_t){
			.min.x = rect.min.x,
			.max.x = rect.max.x - x,
			.min.y = rect.min.y,
			.max.y = rect.max.y,
		};
	}

	if (r)
	{
		*r = (rect2_t){
			.min.x = rect.max.x - x,
			.max.x = rect.max.x,
			.min.y = rect.min.y,
			.max.y = rect.max.y,
		};
	}
}

fn_local void rect2_cut_from_top(rect2_t rect, ui_size_t size, rect2_t *t, rect2_t *b)
{
	float x = ui_size_to_height(rect, size);

	if (b)
	{
		*b = (rect2_t){
			.min.x = rect.min.x,
			.max.x = rect.max.x,
			.min.y = rect.min.y,
			.max.y = rect.max.y - x,
		};
	}

	if (t)
	{
		*t = (rect2_t){
			.min.x = rect.min.x,
			.max.x = rect.max.x,
			.min.y = rect.max.y - x,
			.max.y = rect.max.y,
		};
	}
}

fn_local void rect2_cut_from_bottom(rect2_t rect, ui_size_t size, rect2_t *b, rect2_t *t)
{
	float x = ui_size_to_height(rect, size);

	if (b)
	{
		*b = (rect2_t){
			.min.x = rect.min.x,
			.max.x = rect.max.x,
			.min.y = rect.min.y,
			.max.y = rect.min.y + x,
		};
	}

	if (t)
	{
		*t = (rect2_t){
			.min.x = rect.min.x,
			.max.x = rect.max.x,
			.min.y = rect.min.y + x,
			.max.y = rect.max.y,
		};
	}
}

fn_local void rect2_cut(rect2_t rect, ui_layout_flow_t flow, ui_size_t size, rect2_t *active, rect2_t *remainder)
{
	switch (flow)
	{
		case Flow_north: rect2_cut_from_bottom(rect, size, active, remainder); break;
		case Flow_east:  rect2_cut_from_left  (rect, size, active, remainder); break;
		case Flow_south: rect2_cut_from_top   (rect, size, active, remainder); break;
		case Flow_west:  rect2_cut_from_right (rect, size, active, remainder); break;

		default:
		{
			if (active)    *active    = (rect2_t){0};
			if (remainder) *remainder = rect;
			log(UI, Error, "Called rect2_cut with invalid flow");
		} break;
	}
}

fn_local rect2_t rect2_cut_margins_horizontally(rect2_t rect, ui_size_t size)
{
	rect2_t result = rect;

	rect2_cut_from_left (result, size, NULL, &result);
	rect2_cut_from_right(result, size, NULL, &result);

	return result;
}

fn_local rect2_t rect2_cut_margins_vertically(rect2_t rect, ui_size_t size)
{
	rect2_t result = rect;

	rect2_cut_from_top   (result, size, NULL, &result);
	rect2_cut_from_bottom(result, size, NULL, &result);

	return result;
}
