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
	Flow_justify,
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

fn ui_axis_t ui_flow_axis(ui_layout_flow_t flow)
{
	switch (flow)
	{
		case Flow_east:
		case Flow_west:
		{
			return Axis_x;
		} break;

		case Flow_north:
		case Flow_south:
		{
			return Axis_y;
		} break;
	}

	log(UI, Error, "Invalid flow (0x%X) passed to ui_flow_axis", flow);

	return Axis_x;
}

typedef enum ui_size_kind_t
{
	UiSize_none,
	UiSize_pct,    // w = pct*r_w,    h = pct*r_h
	UiSize_pix,    // w = pix,        h = pix
	UiSize_aspect, // w = aspect*r_h, h = aspect*r_w
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
		.kind  = UiSize_pct,
		.value = pct,
	};
}

fn_local ui_size_t ui_sz_pix(float pix)
{
	return (ui_size_t){
		.kind  = UiSize_pix,
		.value = pix,
	};
}

fn_local ui_size_t ui_sz_aspect(float aspect)
{
	return (ui_size_t){
		.kind  = UiSize_aspect,
		.value = aspect,
	};
}

fn_local float ui_size_to_width(rect2_t rect, ui_size_t size)
{
	float x = 0.0f;

	switch (size.kind)
	{
		case UiSize_none: /* all good! don't do anything! */   break;
		case UiSize_pct:    x = size.value*rect2_width(rect);  break;
		case UiSize_pix:    x = size.value;                    break;
		case UiSize_aspect: x = size.value*rect2_height(rect); break;

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
		case UiSize_none: /* all good! don't do anything! */   break;
		case UiSize_pct:    y = size.value*rect2_height(rect); break;
		case UiSize_pix:    y = size.value;                    break;
		case UiSize_aspect: y = size.value*rect2_width(rect);  break;

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

fn_local rect2_t rect2_cut_margins(rect2_t rect, ui_size_t size)
{
	rect2_t result = rect;
	result = rect2_cut_margins_horizontally(result, size);
	result = rect2_cut_margins_vertically  (result, size);
	return result;
}
