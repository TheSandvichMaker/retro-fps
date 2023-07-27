#include "core/string.h"
#include "core/arena.h"
#include "core/math.h"

#include "dream/render.h"
#include "dream/render_helpers.h"
#include "dream/input.h"
#include "dream/asset.h"
#include "dream/font.h"
#include "dream/debug_ui.h"

ui_t ui;

//
// style stacks
//

float ui_scalar(ui_style_scalar_t scalar)
{
	if (stack_empty(ui.scalars[scalar])) return ui.base_scalars[scalar];
	else                                 return stack_top(ui.scalars[scalar]);
}

void ui_push_scalar(ui_style_scalar_t scalar, float value)
{
    stack_push(ui.scalars[scalar], value);
}

float ui_pop_scalar(ui_style_scalar_t scalar)
{
    return stack_pop(ui.scalars[scalar]);
}

v4_t ui_color(ui_style_color_t color)
{
	if (stack_empty(ui.colors[color])) return ui.base_colors[color];
	else                               return stack_top(ui.colors[color]);
}

void ui_push_color(ui_style_color_t color, v4_t value)
{
    stack_push(ui.colors[color], value);
}

v4_t ui_pop_color(ui_style_color_t color)
{
    return stack_pop(ui.colors[color]);
}

//
// drawing
//

DREAM_INLINE void ui_draw_styled_rect(rect2_t rect, v4_t color)
{
	uint32_t color_packed = pack_color(color);

	r_ui_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS)),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
	});
}

DREAM_INLINE void ui_draw_styled_rect_roundness(rect2_t rect, v4_t color, v4_t roundness)
{
	uint32_t color_packed = pack_color(color);

	r_ui_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = mul(roundness, v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS))),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
	});
}

DREAM_INLINE void ui_draw_styled_rect_outline(rect2_t rect, v4_t color, float width)
{
	uint32_t color_packed = pack_color(color);

	r_ui_rect((r_ui_rect_t){
		.rect         = rect, 
		.roundedness  = v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS)),
		.color_00     = color_packed,
		.color_10     = color_packed,
		.color_11     = color_packed,
		.color_01     = color_packed,
		.inner_radius = width,
	});
}

typedef enum ui_text_op_t
{
	UI_TEXT_OP_BOUNDS,
	UI_TEXT_OP_DRAW,
	UI_TEXT_OP_COUNT,
} ui_text_op_t;

DREAM_INLINE rect2_t ui_text_op(font_atlas_t *font, v2_t p, string_t text, v4_t color, ui_text_op_t op)
{
	rect2_t result = rect2_inverted_infinity();

	uint32_t color_packed = pack_color(color);

	r_immediate_texture(font->texture);
	r_immediate_shader(R_SHADER_TEXT);
	r_immediate_blend_mode(R_BLEND_PREMUL_ALPHA);

	float w = (float)font->texture_w;
	float h = (float)font->texture_h;

	v2_t at = p;
	at.y -= 0.5f*font->descent;

	at.x = roundf(at.x);
	at.y = roundf(at.y);

	for (size_t i = 0; i < text.count; i++)
	{
		char c = text.data[i];

		if (is_newline(c))
		{
			at.x  = p.x;
			at.y -= font->y_advance;
		}
		else
		{
			font_glyph_t *glyph = atlas_get_glyph(font, c);

			float cw = (float)(glyph->max_x - glyph->min_x);
			float ch = (float)(glyph->max_y - glyph->min_y);

			cw *= 0.5f; // oversampling
			ch *= 0.5f; // oversampling

			v2_t point = at;
			point.x += glyph->x_offset;
			point.y += glyph->y_offset;

			point.x = roundf(point.x);
			// point.y = floorf(point.y);

			rect2_t rect = rect2_from_min_dim(point, make_v2(cw, ch));
			result = rect2_union(result, rect);

			if (op == UI_TEXT_OP_DRAW)
			{
				float u0 = (float)glyph->min_x / w;
				float v0 = (float)glyph->min_y / h;
				float u1 = (float)glyph->max_x / w;
				float v1 = (float)glyph->max_y / h;

				uint32_t i0 = r_immediate_vertex(&(vertex_immediate_t) {
					.pos = { point.x, point.y, 0.0f },
					.tex = { u0, v1 },
					.col = color_packed,
				});

				uint32_t i1 = r_immediate_vertex(&(vertex_immediate_t) {
					.pos = { point.x + cw, point.y, 0.0f },
					.tex = { u1, v1 },
					.col = color_packed,
				});

				uint32_t i2 = r_immediate_vertex(&(vertex_immediate_t) {
					.pos = { point.x + cw, point.y + ch, 0.0f },
					.tex = { u1, v0 },
					.col = color_packed,
				});

				uint32_t i3 = r_immediate_vertex(&(vertex_immediate_t) {
					.pos = { point.x, point.y + ch, 0.0f },
					.tex = { u0, v0 },
					.col = color_packed,
				});

				r_immediate_index(i0);
				r_immediate_index(i1);
				r_immediate_index(i2);
				r_immediate_index(i0);
				r_immediate_index(i2);
				r_immediate_index(i3);
			}

			if (i + 1 < text.count)
			{
				char c_next = text.data[i + 1];
				at.x += atlas_get_advance(font, c, c_next);
			}
		}
	}

	r_immediate_flush();

	return result;
}

DREAM_INLINE rect2_t ui_text_bounds(v2_t p, string_t text)
{
	return ui_text_op(&ui.font, p, text, (v4_t){0,0,0,0}, UI_TEXT_OP_BOUNDS);
}

DREAM_INLINE float ui_text_width(string_t text)
{
	rect2_t rect = ui_text_bounds((v2_t){0, 0}, text);
	return rect2_width(rect);
}

DREAM_INLINE float ui_text_height(string_t text)
{
	rect2_t rect = ui_text_bounds((v2_t){0, 0}, text);
	return rect2_height(rect);
}

DREAM_INLINE v2_t ui_text_dim(string_t text)
{
	rect2_t rect = ui_text_bounds((v2_t){0, 0}, text);
	return rect2_dim(rect);
}

DREAM_INLINE rect2_t ui_draw_text(v2_t p, string_t text)
{
	ui_text_op(&ui.font, add(p, make_v2(1.0f, -1.0f)), text, ui_color(UI_COLOR_TEXT_SHADOW), UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(&ui.font, p, text, ui_color(UI_COLOR_TEXT), UI_TEXT_OP_DRAW);
	return result;
}

DREAM_INLINE rect2_t ui_header_text_bounds(v2_t p, string_t text)
{
	return ui_text_op(&ui.header_font, p, text, (v4_t){0,0,0,0}, UI_TEXT_OP_BOUNDS);
}

DREAM_INLINE float ui_header_text_width(string_t text)
{
	rect2_t rect = ui_header_text_bounds((v2_t){0, 0}, text);
	return rect2_width(rect);
}

DREAM_INLINE rect2_t ui_draw_header_text(v2_t p, string_t text)
{
	ui_text_op(&ui.header_font, add(p, make_v2(1.0f, -1.0f)), text, ui_color(UI_COLOR_TEXT_SHADOW), UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(&ui.header_font, p, text, ui_color(UI_COLOR_TEXT), UI_TEXT_OP_DRAW);
	return result;
}

//
// panels core
//

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

//
// animations
//

typedef struct ui_anim_t
{
	float c_t;
	float c_v;
	v4_t  t_target;
	v4_t  t_current;
	v4_t  t_velocity;
} ui_anim_t;

static pool_t ui_animation_state = INIT_POOL(ui_anim_t);
static hash_t ui_animation_index;

ui_anim_t *ui_get_anim(ui_id_t id, v4_t init_value)
{
	ui_anim_t *result = hash_find_object(&ui_animation_index, id.value);

	if (!result)
	{
		result = pool_add(&ui_animation_state);
		result->t_target  = init_value;
		result->t_current = init_value;
		hash_add_object(&ui_animation_index, id.value, result);
	}

	result->c_t = ui_scalar(UI_SCALAR_ANIMATION_STIFFNESS);
	result->c_v = ui_scalar(UI_SCALAR_ANIMATION_DAMPEN);

	return result;
}

float ui_interpolate_f32(ui_id_t id, float target)
{
	ui_anim_t *anim = ui_get_anim(id, v4_from_scalar(target));
	anim->t_target.x = target;

	return anim->t_current.x;
}

float ui_set_f32(ui_id_t id, float target)
{
	ui_anim_t *anim = ui_get_anim(id, v4_from_scalar(target));
	anim->t_target.x  = target;
	anim->t_current.x = target;

	return anim->t_current.x;
}

v4_t ui_interpolate_v4(ui_id_t id, v4_t target)
{
	ui_anim_t *anim = ui_get_anim(id, target);
	anim->t_target = target;

	return anim->t_current;
}

v4_t ui_set_v4(ui_id_t id, v4_t target)
{
	ui_anim_t *anim = ui_get_anim(id, target);
	anim->t_target  = target;
	anim->t_current = target;

	return anim->t_current;
}

//
// widgets
//

static pool_t widgets = INIT_POOL(ui_widget_t);
static hash_t widget_index;

ui_widget_t *ui_get_widget(ui_id_t id)
{
	ui_widget_t *result = hash_find_object(&widget_index, id.value);

	if (!result)
	{
		result = pool_add(&widgets);
		result->id                  = id;
		result->created_frame_index = ui.frame_index;
		hash_add_object(&widget_index, id.value, result);
	}

	ASSERT(result->id.value == id.value);

	result->last_touched_frame_index = ui.frame_index;
	return result;
}

bool ui_widget_is_new(ui_widget_t *widget)
{
	return widget->created_frame_index == ui.frame_index;
}

//
//
//

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
				a += ui_text_width(label);
			} break;

			case UI_CUT_TOP:
			case UI_CUT_BOTTOM:
			{
				a += ui.font.font_height;
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
		ui.font_data        = fs_read_entire_file(&ui.arena, S("gamedata/fonts/NotoSans/NotoSans-Regular.ttf"));
		ui.header_font_data = fs_read_entire_file(&ui.arena, S("gamedata/fonts/NotoSans/NotoSans-Bold.ttf"));
		ui_set_font_size(18.0f);

		v4_t background    = make_v4(0.20f, 0.15f, 0.17f, 1.0f);
		v4_t background_hi = make_v4(0.22f, 0.18f, 0.18f, 1.0f);
		v4_t foreground    = make_v4(0.33f, 0.28f, 0.28f, 1.0f);

		v4_t hot    = make_v4(0.25f, 0.45f, 0.40f, 1.0f);
		v4_t active = make_v4(0.30f, 0.55f, 0.50f, 1.0f);
		v4_t fired  = make_v4(0.40f, 0.60f, 0.50f, 1.0f);

        ui.base_scalars[UI_SCALAR_ANIMATION_RATE     ] = 40.0f;
        ui.base_scalars[UI_SCALAR_ANIMATION_STIFFNESS] = 512.0f;
        ui.base_scalars[UI_SCALAR_ANIMATION_DAMPEN   ] = 32.0f;
        ui.base_scalars[UI_SCALAR_HOVER_LIFT         ] = 1.5f;
		ui.base_scalars[UI_SCALAR_WIDGET_MARGIN      ] = 0.75f;
		ui.base_scalars[UI_SCALAR_TEXT_MARGIN        ] = 2.2f;
		ui.base_scalars[UI_SCALAR_ROUNDEDNESS        ] = 2.5f;
		ui.base_scalars[UI_SCALAR_TEXT_ALIGN_X       ] = 0.5f;
		ui.base_scalars[UI_SCALAR_TEXT_ALIGN_Y       ] = 0.5f;
		ui.base_scalars[UI_SCALAR_SCROLLBAR_WIDTH    ] = 12.0f;
        ui.base_scalars[UI_SCALAR_SLIDER_HANDLE_RATIO] = 3.0f;
        ui.base_scalars[UI_SCALAR_SLIDER_HANDLE_RATIO] = 3.0f;
		ui.base_colors [UI_COLOR_TEXT                ] = make_v4(0.95f, 0.90f, 0.85f, 1.0f);
		ui.base_colors [UI_COLOR_TEXT_SHADOW         ] = make_v4(0.00f, 0.00f, 0.00f, 0.50f);
		ui.base_colors [UI_COLOR_WIDGET_SHADOW       ] = make_v4(0.00f, 0.00f, 0.00f, 0.20f);
		ui.base_colors [UI_COLOR_WINDOW_BACKGROUND   ] = background;
		ui.base_colors [UI_COLOR_WINDOW_TITLE_BAR    ] = make_v4(0.45f, 0.25f, 0.25f, 1.0f);
		ui.base_colors [UI_COLOR_WINDOW_TITLE_BAR_HOT] = make_v4(0.45f, 0.22f, 0.22f, 1.0f);
		ui.base_colors [UI_COLOR_WINDOW_CLOSE_BUTTON ] = make_v4(0.35f, 0.15f, 0.15f, 1.0f);
		ui.base_colors [UI_COLOR_PROGRESS_BAR_EMPTY  ] = background_hi;
		ui.base_colors [UI_COLOR_PROGRESS_BAR_FILLED ] = hot;
		ui.base_colors [UI_COLOR_BUTTON_IDLE         ] = foreground;
		ui.base_colors [UI_COLOR_BUTTON_HOT          ] = hot;
		ui.base_colors [UI_COLOR_BUTTON_ACTIVE       ] = active;
		ui.base_colors [UI_COLOR_BUTTON_FIRED        ] = fired;
		ui.base_colors [UI_COLOR_SLIDER_BACKGROUND   ] = background_hi;
		ui.base_colors [UI_COLOR_SLIDER_FOREGROUND   ] = foreground;
		ui.base_colors [UI_COLOR_SLIDER_HOT          ] = hot;
		ui.base_colors [UI_COLOR_SLIDER_ACTIVE       ] = active;

		ui.initialized = true;
	}

	ui.frame_index += 1;

	for (pool_iter_t it = pool_iter(&widgets); 
		 pool_iter_valid(&it); 
		 pool_iter_next(&it))
	{
		ui_widget_t *widget = it.data;

		if (widget->last_touched_frame_index + 1 < ui.frame_index)
		{
			hash_rem     (&widget_index, widget->id.value);
			pool_rem_item(&widgets, widget);
		}
	}

	for (pool_iter_t it = pool_iter(&ui_animation_state); 
		 pool_iter_valid(&it); 
		 pool_iter_next(&it))
	{
		ui_anim_t *anim = it.data;

		float c_t = anim->c_t;
		float c_v = anim->c_v;

		v4_t t_target   = anim->t_target;
		v4_t t_current  = anim->t_current;
		v4_t t_velocity = anim->t_velocity;

		v4_t accel_t = mul( c_t, sub(t_target, t_current));
		v4_t accel_v = mul(-c_v, t_velocity);
		v4_t accel = add(accel_t, accel_v);

		t_velocity = add(t_velocity, mul(ui.dt, accel));
		t_current  = add(t_current,  mul(ui.dt, t_velocity));

		anim->t_current  = t_current;
		anim->t_velocity = t_velocity;
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

void ui_set_font_size(float size)
{
	if (ui.font.initialized)
		destroy_font_atlas(&ui.font);

	if (ui.header_font.initialized)
		destroy_font_atlas(&ui.header_font);

	font_range_t ranges[] = {
		{ .start = ' ', .end = '~' },
	};

	ui.font        = make_font_atlas_from_memory(ui.font_data, ARRAY_COUNT(ranges), ranges, size);
	ui.header_font = make_font_atlas_from_memory(ui.header_font_data, ARRAY_COUNT(ranges), ranges, 1.5f*size);
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

DREAM_INLINE bool ui_check_hovered(rect2_t rect)
{
	bool result = false;
	if (rect2_contains_point(rect, ui.mouse_p))
	{
		result = true;
		ui.hovered = true;
	}
	return result;
}

DREAM_INLINE v2_t ui_text_center_p(rect2_t rect, string_t text)
{
    float text_width  = ui_text_width(text);
    float text_height = 0.5f*ui.font.y_advance;

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

    float text_width  = ui_text_width(text);
    float text_height = 0.5f*ui.font.y_advance;

    float rect_width  = rect2_width(rect);
    float rect_height = rect2_height(rect);

    v2_t result = {
        .x = rect.min.x + align_x*rect_width  - align_x*text_width,
        .y = rect.min.y + align_y*rect_height - align_y*text_height,
    };

    return result;
}

DREAM_INLINE v2_t ui_header_text_align_p(rect2_t rect, string_t text)
{
    float align_x = ui_scalar(UI_SCALAR_TEXT_ALIGN_X);
    float align_y = ui_scalar(UI_SCALAR_TEXT_ALIGN_Y);

    float text_width  = ui_header_text_width(text);
    float text_height = 0.5f*ui.header_font.y_advance;

    float rect_width  = rect2_width(rect);
    float rect_height = rect2_height(rect);

    v2_t result = {
        .x = rect.min.x + align_x*rect_width  - align_x*text_width,
        .y = rect.min.y + align_y*rect_height - align_y*text_height,
    };

    return result;
}


void ui_window_begin(ui_window_t *window)
{
	ASSERT(ui.initialized);

	ui_id_t id = window->id = ui_id_pointer(&window);

	rect2_t rect = window->rect;
    ui_check_hovered(rect);

	rect2_t bar = ui_add_top(&rect, ui.header_font.font_height + 2.0f*ui_scalar(UI_SCALAR_TEXT_MARGIN));
	bool bar_hovered = ui_check_hovered(bar);

    rect2_t both = rect2_union(bar, rect);
    uint32_t interaction = ui_button_behaviour(id, both);

    if (interaction & UI_PRESSED)
    {
        ui.drag_anchor = sub(rect.min, ui.mouse_p);
    }

    if (ui_is_active(id))
    {
        window->rect = rect2_reposition_min(window->rect, add(ui.mouse_p, ui.drag_anchor));
    }

	{
		uint32_t color_packed = pack_color(ui_color(UI_COLOR_WINDOW_BACKGROUND));

		r_ui_rect((r_ui_rect_t){
			.rect        = both, 
			.roundedness = mul(make_v4(2, 2, 2, 2), v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS))),
			.color_00    = color_packed,
			.color_10    = color_packed,
			.color_11    = color_packed,
			.color_01    = color_packed,
			.shadow_radius = 32.0f,
			.shadow_amount = 0.15f,
		});
	}

    // NOTE: the panel will push a very similar view right away but we need it for the rendering here
	r_push_view_screenspace_clip_rect(both);

	v4_t bar_color = bar_hovered ? ui_color(UI_COLOR_WINDOW_TITLE_BAR_HOT) : ui_color(UI_COLOR_WINDOW_TITLE_BAR);
	bar_color = ui_interpolate_v4(ui_child_id(id, S("bar_hover")), bar_color);

	ui_draw_styled_rect_roundness(bar, bar_color, make_v4(2, 0, 2, 0));
	ui_draw_styled_rect_roundness(rect, ui_color(UI_COLOR_WINDOW_BACKGROUND), make_v4(0, 2, 0, 2));

	ui_set_next_rect(ui_cut_right(&bar, ui_label_width(S("Close"))));

	window->open = true;

	// TODO: Have parent IDs propagate automatically?
	ui_id_t close_id = ui_child_id(id, S("close"));
	ui_set_next_id(close_id);

	UI_SCALAR(UI_SCALAR_WIDGET_MARGIN, 0.0f)
	UI_COLOR(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_WINDOW_CLOSE_BUTTON))
	if (ui_button(S("Close")))
	{
		window->open = false;
	}

    ui_id_t panel_id = ui_child_id(id, S("panel"));
	ui_draw_header_text(add(bar.min, make_v2(ui_scalar(UI_SCALAR_TEXT_MARGIN), ui_scalar(UI_SCALAR_TEXT_MARGIN))), window->name);
	ui_panel_begin_ex(panel_id, rect, UI_PANEL_SCROLLABLE_VERT);
}

void ui_window_end(ui_window_t *window)
{
	ASSERT(ui.initialized);

	(void)window;

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
			}

			// hmm
			pct = ui_interpolate_f32(scrollbar_id, pct);

			widget->scroll_offset_y = pct*(widget->scrollable_height_y - rect2_height(inner_rect));

			float height_exclusive = scroll_area_height - handle_size;
			float handle_offset = pct*height_exclusive;

			rect2_t top    = ui_cut_top(&scroll_area, handle_offset);
			rect2_t handle = ui_cut_top(&scroll_area, handle_size);
			rect2_t bot    = scroll_area;

			ui_button_behaviour(scrollbar_id, handle);

			v4_t color = ui_color(UI_COLOR_SLIDER_FOREGROUND);

			ui_draw_styled_rect(top, ui_color(UI_COLOR_SLIDER_BACKGROUND));
			ui_draw_styled_rect(handle, color);
			ui_draw_styled_rect(bot, ui_color(UI_COLOR_SLIDER_BACKGROUND));

			offset_y = roundf(widget->scroll_offset_y);
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
	ui_draw_text(ui_text_align_p(text_rect, label), label);
}

void ui_header(string_t label)
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
				a += rect2_width(ui_text_op(&ui.header_font, (v2_t){0, 0}, label, (v4_t){0,0,0,0}, UI_TEXT_OP_BOUNDS));
			} break;

			case UI_CUT_TOP:
			case UI_CUT_BOTTOM:
			{
				a += ui.header_font.font_height;
			} break;
		}

		rect = ui_do_cut((ui_cut_t){ .side = panel->layout_direction, .rect = &panel->rect }, a);
	}

	rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = ui_shrink(&rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));
	ui_draw_header_text(ui_header_text_align_p(text_rect, label), label);
}

float ui_label_width(string_t label)
{
    return ui_text_width(label) + 2.0f*ui_widget_padding();
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

	rect2_t tray   = rect;
	rect2_t filled = ui_cut_left(&rect, progress*width);

	ui_draw_styled_rect(tray, ui_color(UI_COLOR_PROGRESS_BAR_EMPTY));
	ui_draw_styled_rect(filled, ui_color(UI_COLOR_PROGRESS_BAR_FILLED));

	ui_draw_text(ui_text_align_p(text_rect, label), label);
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

DREAM_INLINE v4_t ui_animate_colors(ui_id_t id, ui_widget_state_t state, uint32_t interaction, v4_t cold, v4_t hot, v4_t active, v4_t fired)
{
	ui_id_t color_id = ui_child_id(id, S("color"));

	if (interaction & UI_FIRED)
	{
		ui_set_v4(color_id, fired);
	}

	v4_t target = cold;
	switch (state)
	{
		case UI_WIDGET_STATE_COLD:   target = cold;   break;
		case UI_WIDGET_STATE_HOT:    target = hot;    break;
		case UI_WIDGET_STATE_ACTIVE: target = active; break;
	}

	v4_t interp_color = ui_interpolate_v4(color_id, target);
	return interp_color;
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

	v4_t color = ui_animate_colors(id, state, interaction, 
								   ui_color(UI_COLOR_BUTTON_IDLE),
								   ui_color(UI_COLOR_BUTTON_HOT),
								   ui_color(UI_COLOR_BUTTON_ACTIVE),
								   ui_color(UI_COLOR_BUTTON_FIRED));

	float hover_lift = ui_is_hot(id) && !ui_is_active(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);

	rect2_t shadow = rect;

	rect      = rect2_add_offset(rect,      make_v2(0.0f, hover_lift));
	text_rect = rect2_add_offset(text_rect, make_v2(0.0f, hover_lift));

	ui_draw_styled_rect(shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_styled_rect(rect, color);

	ui_draw_text(ui_text_center_p(text_rect, label), label);

	return result;
}

DREAM_INLINE float ui_roundedness_ratio(rect2_t rect)
{
	float w = rect2_width (rect);
	float h = rect2_height(rect);
	float c = 0.5f*min(w, h);
	float ratio = ui_scalar(UI_SCALAR_ROUNDEDNESS) / c;
	return ratio;
}

DREAM_INLINE float ui_roundedness_ratio_to_abs(rect2_t rect, float ratio)
{
	float w = rect2_width (rect);
	float h = rect2_height(rect);
	float c = 0.5f*min(w, h);
	float abs = c*ratio;
	return abs;
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

	rect2_t outer_box_rect = ui_cut_left(&rect, rect2_height(rect));

	float outer_rect_rel_width = 0.15f;
	float outer_rect_c = outer_rect_rel_width*min(rect2_width(outer_box_rect), rect2_height(outer_box_rect));

	rect2_t inner_box_rect = ui_shrink(&outer_box_rect, outer_rect_c*1.5f);
	rect2_t label_rect = ui_cut_left(&rect, ui_text_width(label));
	label_rect = ui_shrink(&label_rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	uint32_t interaction = ui_button_behaviour(id, outer_box_rect);
	result = interaction & UI_FIRED;

	if (result)
	{
		*value = !*value;
	}

	ui_widget_state_t state = ui_get_widget_state(id);

	if (*value && state == UI_WIDGET_STATE_COLD)
		state = UI_WIDGET_STATE_ACTIVE;

	v4_t color = ui_animate_colors(id, state, interaction, 
								   ui_color(UI_COLOR_BUTTON_IDLE),
								   ui_color(UI_COLOR_BUTTON_HOT),
								   ui_color(UI_COLOR_BUTTON_ACTIVE),
								   ui_color(UI_COLOR_BUTTON_FIRED));

	float hover_lift = ui_is_hot(id) && !ui_is_active(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);

	// clamp roundedness to avoid the checkbox looking like a radio button and
	// transfer roundedness of outer rect to inner rect so the countours match
	float roundedness_ratio = min(0.66f, ui_roundedness_ratio(outer_box_rect));
	float outer_roundedness = ui_roundedness_ratio_to_abs(outer_box_rect, roundedness_ratio);
	float inner_roundedness = ui_roundedness_ratio_to_abs(inner_box_rect, roundedness_ratio);

	UI_SCALAR(UI_SCALAR_ROUNDEDNESS, outer_roundedness)
	{
		rect2_t outer_box_rect_shadow = outer_box_rect;
		outer_box_rect = rect2_add_offset(outer_box_rect, make_v2(0, hover_lift));

		ui_draw_styled_rect_outline(outer_box_rect_shadow, ui_color(UI_COLOR_WIDGET_SHADOW), outer_rect_c);
		ui_draw_styled_rect_outline(outer_box_rect, color, outer_rect_c);
	}

	if (*value) 
	{
		UI_SCALAR(UI_SCALAR_ROUNDEDNESS, inner_roundedness)
		{
			rect2_t inner_box_rect_shadow = inner_box_rect;
			inner_box_rect = rect2_add_offset(inner_box_rect, make_v2(0, hover_lift));

			ui_draw_styled_rect(inner_box_rect_shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
			ui_draw_styled_rect(inner_box_rect, color);
		}
	}

	ui_draw_text(label_rect.min, label);

	return result;
}

bool ui_radio(string_t label, int *value, int count, string_t *labels)
{
    bool result = false;

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = ui_cut_top(&panel->rect, ui.font.font_height + ui_widget_padding());
	}

	rect2_t label_rect = ui_cut_left(&rect, ui_text_width(label) + ui_widget_padding());
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

	ui_draw_text(label_rect.min, label);

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
		rect = ui_cut_top(&panel->rect, ui.font.font_height + ui_widget_padding());
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

	pct = saturate(pct);
	pct = ui_interpolate_f32(id, pct);

	float width_exclusive = width - handle_width;
	float handle_offset = pct*width_exclusive;

	rect2_t left   = ui_cut_left(&rect, handle_offset);
	rect2_t handle = ui_cut_left(&rect, handle_width);
	rect2_t right  = rect;

	(void)left;
	(void)right;

	uint32_t interaction = ui_button_behaviour(id, handle);
	ui_widget_state_t state = ui_get_widget_state(id);

	v4_t color = ui_animate_colors(id, state, interaction, 
								   ui_color(UI_COLOR_SLIDER_FOREGROUND),
								   ui_color(UI_COLOR_SLIDER_HOT),
								   ui_color(UI_COLOR_SLIDER_ACTIVE),
								   ui_color(UI_COLOR_SLIDER_ACTIVE));

	float hover_lift = ui_is_hot(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);

	rect2_t shadow = handle;
	handle = rect2_add_offset(handle, make_v2(0, hover_lift));

	// r_immediate_clip_rect(slider_body); TODO: ui rect clip rects?
	ui_draw_styled_rect(slider_body, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	ui_draw_styled_rect(shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_styled_rect(handle, color);
	ui_draw_styled_rect(right, ui_color(UI_COLOR_SLIDER_BACKGROUND));

	ui_draw_text(label_rect.min, label);

	string_t value_text = Sf("%.03f", *v);

	v2_t text_p = ui_text_center_p(slider_body, value_text);
	ui_draw_text(text_p, value_text);
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
		rect = ui_cut_top(&panel->rect, ui.font.font_height + ui_widget_padding());
	}

	float w = rect2_width(rect);

	rect2_t label_rect = ui_cut_left(&rect, 0.5f*w); // (float)label.count*ui.font.cw + ui_widget_padding());
	label_rect = ui_shrink(&label_rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN) + ui_scalar(UI_SCALAR_TEXT_MARGIN));

	rect2_t sub_rect = ui_cut_left (&rect, rect2_height(rect));
	rect2_t add_rect = ui_cut_right(&rect, rect2_height(rect));

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
		ui_set_f32(id, pct);
	}
	else
	{
		int value = *v;
		pct = (float)(value - min) / (float)(max - min); // TODO: protect against division by zero, think about min > max case
	}

	pct = saturate(pct);

	float hover_lift = ui_is_hot(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);

	float pct_interp = pct;
	if (!ui_is_active(id))
	{
		pct_interp = ui_interpolate_f32(id, pct);
	}

	float width_exclusive = width - handle_width;
	float handle_offset = pct_interp*width_exclusive;

	rect2_t left   = ui_cut_left(&rect, handle_offset);
	rect2_t handle = ui_cut_left(&rect, handle_width);
	rect2_t right  = rect;

	uint32_t interaction = ui_button_behaviour(id, handle);

	rect2_t handle_no_offset = handle;
	handle = rect2_add_offset(handle, make_v2(0, hover_lift));

	ui_widget_state_t state = ui_get_widget_state(id);

	v4_t color = ui_animate_colors(id, state, interaction, 
								   ui_color(UI_COLOR_SLIDER_FOREGROUND),
								   ui_color(UI_COLOR_SLIDER_HOT),
								   ui_color(UI_COLOR_SLIDER_ACTIVE),
								   ui_color(UI_COLOR_SLIDER_ACTIVE));

	rect2_t slider_clip = slider_body;
	slider_clip.max.y += 2.0f;

	(void)left;
	(void)right;

	ui_draw_styled_rect(slider_body, ui_color(UI_COLOR_SLIDER_BACKGROUND));

	ui_draw_styled_rect(handle_no_offset, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_styled_rect(handle, color);

	ui_draw_text(label_rect.min, label);

	string_t value_text = Sf("%d", *v);

	v2_t text_p = ui_text_center_p(slider_body, value_text);
	ui_draw_text(text_p, value_text);

	return *v != init_value;
}
