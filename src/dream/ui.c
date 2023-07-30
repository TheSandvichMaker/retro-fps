#include "core/math.h"
#include "dream/ui.h"

//
// IDs
//

ui_id_t ui_id(string_t string)
{
	ui_id_t parent = UI_ID_NULL;

	if (!stack_empty(ui.id_stack))
	{
		parent = stack_top(ui.id_stack);
	}

	return ui_child_id(string, parent);
}

ui_id_t ui_child_id(string_t string, ui_id_t parent)
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

ui_id_t ui_id_pointer(void *pointer)
{
	ui_id_t result = {
		.value = (uintptr_t)pointer,
	};
	return result;
}

void ui_set_next_id(ui_id_t id)
{
	ui.next_id = id;
}

void ui_push_id(ui_id_t id)
{
	stack_push(ui.id_stack, id);
}

void ui_pop_id(void)
{
	stack_pop(ui.id_stack);
}

//
// Input
//

void ui_submit_mouse_buttons(platform_mouse_buttons_t buttons, bool pressed)
{
	ui.input.mouse_buttons_down = TOGGLE_BIT(ui.input.mouse_buttons_down, buttons, pressed);

	if (pressed)
	{
		ui.input.mouse_buttons_pressed |= buttons;
	}
	else
	{
		ui.input.mouse_buttons_released |= buttons;
	}
}

void ui_submit_mouse_wheel(float wheel)
{
	ui.input.mouse_wheel += wheel;
}

void ui_submit_mouse_p(v2_t p)
{
	ui.input.mouse_p = p;
}

void ui_submit_mouse_dp(v2_t dp)
{
	ui.input.mouse_dp = dp;
}

bool ui_mouse_buttons_down(platform_mouse_buttons_t buttons)
{
	return !!(ui.input.mouse_buttons_down & buttons);
}

bool ui_mouse_buttons_pressed(platform_mouse_buttons_t buttons)
{
	return !!(ui.input.mouse_buttons_pressed & buttons);
}

bool ui_mouse_buttons_released(platform_mouse_buttons_t buttons)
{
	return !!(ui.input.mouse_buttons_released & buttons);
}

//
// Panel
//

ui_panel_t *ui_push_panel(ui_id_t id, rect2_t rect, ui_panel_flags_t flags)
{
	if (!ui.panels.first_free_panel)
	{
		ui.panels.first_free_panel = m_alloc_struct_nozero(&ui.arena, ui_panel_t);
		ui.panels.first_free_panel->next_free = NULL;
	}

	ui_panel_t *panel = ui.panels.first_free_panel;
	ui.panels.first_free_panel = panel->next_free;

	zero_struct(panel);

	panel->id               = id;
	panel->flags            = flags;
	panel->layout_direction = RECT2_CUT_TOP;
	panel->rect_init        = rect;
	panel->rect_layout      = rect;

	panel->parent = ui.panels.current_panel;
	ui.panels.current_panel = panel;

	return panel;
}

void ui_pop_panel(void)
{
	if (ALWAYS(ui.panels.current_panel))
	{
		ui_panel_t *panel = ui.panels.current_panel;
		ui.panels.current_panel = ui.panels.current_panel->parent;

		panel->next_free = ui.panels.first_free_panel;
		ui.panels.first_free_panel = panel;
	}
}

ui_panel_t *ui_panel(void)
{
	static ui_panel_t dummy_panel = { 0 };

	ui_panel_t *result = &dummy_panel;

	if (ALWAYS(ui.panels.current_panel))
	{
		result = ui.panels.current_panel;
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
	return &panel->rect_layout;
}

void ui_set_layout_direction(rect2_cut_side_t side)
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
			case RECT2_CUT_LEFT:
			case RECT2_CUT_RIGHT:
			{
				size = rect2_width(panel->rect_layout) / item_count;
			} break;

			case RECT2_CUT_TOP:
			case RECT2_CUT_BOTTOM:
			{
				size = rect2_height(panel->rect_layout) / item_count;
			} break;
		}
	}

	return size;
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

rect2_t ui_default_label_rect(font_atlas_t *font, string_t label)
{
	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();

		float a = ui_widget_padding();

		switch (panel->layout_direction)
		{
			case RECT2_CUT_LEFT:
			case RECT2_CUT_RIGHT:
			{
				a += ui_text_width(font, label);
			} break;

			case RECT2_CUT_TOP:
			case RECT2_CUT_BOTTOM:
			{
				a += font->font_height;
			} break;
		}

		rect = rect2_do_cut((rect2_cut_t){ .side = panel->layout_direction, .rect = &panel->rect_layout }, a);
	}
	return rect;
}

//
// Windows
//

void ui_add_window(ui_window_t *window)
{
	dll_push_back(ui.windows.first_window, ui.windows.last_window, window);
}

void ui_remove_window(ui_window_t *window)
{
	dll_remove(ui.windows.first_window, ui.windows.last_window, window);
}

void ui_bring_window_to_front(ui_window_t *window)
{
	dll_remove   (ui.windows.first_window, ui.windows.last_window, window);
	dll_push_back(ui.windows.first_window, ui.windows.last_window, window);
}

void ui_send_window_to_back(ui_window_t *window)
{
	dll_remove    (ui.windows.first_window, ui.windows.last_window, window);
	dll_push_front(ui.windows.first_window, ui.windows.last_window, window);
}

void ui_focus_window(ui_window_t *window)
{
	ui.windows.focus_window = window;
	ui.has_focus            = true;
}

void ui_process_windows(void)
{
	ui_window_t *hovered_window = NULL;

	r_push_view_screenspace();

	for (ui_window_t *window = ui.windows.first_window;
		 window;
		 window = window->next)
	{
		if (!window->open)
			continue;

		ui_id_t id = ui_id_pointer(window);

		ui_push_id(id);
		{
			// handle dragging and resizing first to remove frame delay

			if (ui_is_active(id))
			{
				v2_t new_p = add(ui.input.mouse_p, ui.drag_offset);
				window->rect = rect2_reposition_min(window->rect, new_p); 
			}

			ui_id_t id_n  = ui_id(S("tray_id_n"));
			ui_id_t id_e  = ui_id(S("tray_id_e"));
			ui_id_t id_s  = ui_id(S("tray_id_s"));
			ui_id_t id_w  = ui_id(S("tray_id_w"));
			ui_id_t id_ne = ui_id(S("tray_id_ne"));
			ui_id_t id_nw = ui_id(S("tray_id_nw"));
			ui_id_t id_se = ui_id(S("tray_id_se"));
			ui_id_t id_sw = ui_id(S("tray_id_sw"));

			if (ui_is_hot(id_n))  { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_NS; };
			if (ui_is_hot(id_e))  { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_EW; };
			if (ui_is_hot(id_s))  { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_NS; };
			if (ui_is_hot(id_w))  { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_EW; };
			if (ui_is_hot(id_ne)) { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_NESW; };
			if (ui_is_hot(id_nw)) { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_NWSE; };
			if (ui_is_hot(id_se)) { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_NWSE; };
			if (ui_is_hot(id_sw)) { ui.input.cursor_reset_delay = 1; ui.input.cursor = PLATFORM_CURSOR_RESIZE_NESW; };

			ui_rect_edge_t tray_region = 0;

			if (ui_is_active(id_n))  tray_region = UI_RECT_EDGE_N;
			if (ui_is_active(id_e))  tray_region = UI_RECT_EDGE_E;
			if (ui_is_active(id_s))  tray_region = UI_RECT_EDGE_S;
			if (ui_is_active(id_w))  tray_region = UI_RECT_EDGE_W;
			if (ui_is_active(id_ne)) tray_region = UI_RECT_EDGE_N|UI_RECT_EDGE_E;
			if (ui_is_active(id_nw)) tray_region = UI_RECT_EDGE_N|UI_RECT_EDGE_W;
			if (ui_is_active(id_se)) tray_region = UI_RECT_EDGE_S|UI_RECT_EDGE_E;
			if (ui_is_active(id_sw)) tray_region = UI_RECT_EDGE_S|UI_RECT_EDGE_W;

			if (tray_region)
			{
				v2_t delta = sub(ui.input.mouse_p, ui.input.mouse_pressed_p);

				window->rect = ui.resize_original_rect;
				if (tray_region & UI_RECT_EDGE_E) window->rect = rect2_extend_right(window->rect,  delta.x);
				if (tray_region & UI_RECT_EDGE_W) window->rect = rect2_extend_left (window->rect, -delta.x);
				if (tray_region & UI_RECT_EDGE_N) window->rect = rect2_extend_up   (window->rect,  delta.y);
				if (tray_region & UI_RECT_EDGE_S) window->rect = rect2_extend_down (window->rect, -delta.y);

				rect2_t min_rect = rect2_from_min_dim(window->rect.min, make_v2(64, 64));
				window->rect = rect2_union(window->rect, min_rect);
			}

			// ---

			rect2_t rect = window->rect;

			float   title_bar_height = ui_header_font_height() + ui_widget_padding();
			rect2_t title_bar = rect2_add_top(rect, title_bar_height);

			rect2_t total = rect2_union(title_bar, rect);

			// drag and resize behaviour

			float tray_width = 6.0f;

			rect2_t interact_total = rect2_extend(total, 0.5f*tray_width);
			ui_interaction_t interaction = ui_widget_behaviour(id, interact_total);

			if (ui_hover_rect(interact_total))
			{
				hovered_window = window;
			}

			if (interaction & UI_PRESSED)
			{
				ui.drag_offset = sub(window->rect.min, ui.input.mouse_p);
			}

			rect2_t tray_init = interact_total;
			rect2_t tray_e  = rect2_cut_right (&tray_init, tray_width);
			rect2_t tray_w  = rect2_cut_left  (&tray_init, tray_width);
			rect2_t tray_n  = rect2_cut_top   (&tray_init, tray_width);
			rect2_t tray_s  = rect2_cut_bottom(&tray_init, tray_width);
			rect2_t tray_ne = rect2_cut_top   (&tray_e,    tray_width);
			rect2_t tray_nw = rect2_cut_top   (&tray_w,    tray_width);
			rect2_t tray_se = rect2_cut_bottom(&tray_e,    tray_width);
			rect2_t tray_sw = rect2_cut_bottom(&tray_w,    tray_width);

			ui_interaction_t tray_interaction = 0;
			tray_interaction |= ui_widget_behaviour(id_n,  tray_n);
			tray_interaction |= ui_widget_behaviour(id_e,  tray_e);
			tray_interaction |= ui_widget_behaviour(id_s,  tray_s);
			tray_interaction |= ui_widget_behaviour(id_w,  tray_w);
			tray_interaction |= ui_widget_behaviour(id_ne, tray_ne);
			tray_interaction |= ui_widget_behaviour(id_nw, tray_nw);
			tray_interaction |= ui_widget_behaviour(id_se, tray_se);
			tray_interaction |= ui_widget_behaviour(id_sw, tray_sw);

			if (tray_interaction & UI_PRESSED)
			{
				ui.resize_original_rect = window->rect;
			}

			// draw window

			string_t title = string_from_storage(window->title);

			bool has_focus = window == ui.windows.focus_window;

			float focus_t = ui_interpolate_f32(ui_id(S("focus")), has_focus);
			float shadow_amount = lerp(0.15f, 0.25f, focus_t);

			v4_t  title_bar_color           = ui_color(UI_COLOR_WINDOW_TITLE_BAR);
			float title_bar_luma            = luminance(title_bar_color.xyz);
			v4_t  title_bar_color_greyscale = make_v4(title_bar_luma, title_bar_luma, title_bar_luma, 1.0f);

			v4_t  interpolated_title_bar_color = v4_lerps(title_bar_color_greyscale, title_bar_color, focus_t);

			ui_draw_rect_roundedness_shadow(rect2_shrink(total, 1.0f), make_v4(0, 0, 0, 0), make_v4(4, 4, 4, 4), shadow_amount, 32.0f);
			ui_draw_rect_roundedness(title_bar, interpolated_title_bar_color, make_v4(2, 0, 2, 0));
			ui_draw_rect_roundedness(rect, ui_color(UI_COLOR_WINDOW_BACKGROUND), make_v4(0, 2, 0, 2));
			ui_draw_text(&ui.style.header_font, ui_text_center_p(&ui.style.header_font, title_bar, title), title);
			ui_draw_rect_roundedness_outline(total, ui_color(UI_COLOR_WINDOW_OUTLINE), make_v4(2, 2, 2, 2), 2.0f);

			// handle window contents

			float margin = max(ui_scalar(UI_SCALAR_WIDGET_MARGIN), 0.5f*ui_scalar(UI_SCALAR_ROUNDEDNESS));
			rect = rect2_shrink(rect, margin);
			rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WINDOW_MARGIN));

			// TODO: Custom scrollbar rendering for windows?
			ui_panel_begin_ex(ui_id(S("panel")), rect, UI_PANEL_SCROLLABLE_VERT);

			window->draw_proc(window->user_data);

			ui_panel_end();
		}
		ui_pop_id();
	}

	r_pop_view();

	if (hovered_window)
	{
		ui.hovered = true;
	}

	if (ui_mouse_buttons_pressed(PLATFORM_MOUSE_BUTTON_ANY))
	{
		if (hovered_window)
		{
			ui_bring_window_to_front(hovered_window);
		}

		ui.windows.focus_window = hovered_window;
	}
}

//
// Style
//

ui_anim_t *ui_get_anim(ui_id_t id, v4_t init_value)
{
	ui_anim_t *result = hash_find_object(&ui.style.animation_index, id.value);

	if (!result)
	{
		result = pool_add(&ui.style.animation_state);
		result->t_target  = init_value;
		result->t_current = init_value;
		hash_add_object(&ui.style.animation_index, id.value, result);
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
	anim->t_target.x   = target;
	anim->t_current.x  = target;
	anim->t_velocity.x = 0.0f;

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
	anim->t_target   = target;
	anim->t_current  = target;
	anim->t_velocity = make_v4(0, 0, 0, 0);

	return anim->t_current;
}

float ui_scalar(ui_style_scalar_t scalar)
{
	if (stack_empty(ui.style.scalars[scalar])) return ui.style.base_scalars[scalar];
	else                                       return stack_top(ui.style.scalars[scalar]);
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
	if (stack_empty(ui.style.colors[color])) return ui.style.base_colors[color];
	else                                     return stack_top(ui.style.colors[color]);
}

void ui_push_color(ui_style_color_t color, v4_t value)
{
    stack_push(ui.style.colors[color], value);
}

v4_t ui_pop_color(ui_style_color_t color)
{
    return stack_pop(ui.style.colors[color]);
}

void ui_set_font_height(float size)
{
	if (ui.style.font.initialized)
		destroy_font_atlas(&ui.style.font);

	if (ui.style.header_font.initialized)
		destroy_font_atlas(&ui.style.header_font);

	font_range_t ranges[] = {
		{ .start = ' ', .end = '~' },
	};

	ui.style.font        = make_font_atlas_from_memory(ui.style.font_data, ARRAY_COUNT(ranges), ranges, size);
	ui.style.header_font = make_font_atlas_from_memory(ui.style.header_font_data, ARRAY_COUNT(ranges), ranges, 1.5f*size);
}

float ui_font_height(void)
{
	return ui.style.font.font_height;
}

float ui_header_font_height(void)
{
	return ui.style.header_font.font_height;
}

//
// Draw
//

rect2_t ui_text_op(font_atlas_t *font, v2_t p, string_t text, v4_t color, ui_text_op_t op)
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

			rect2_t rect = rect2_from_min_dim(point, make_v2(cw, ch));
			result = rect2_union(result, rect);

			if (op == UI_TEXT_OP_DRAW)
			{
				float u0 = (float)glyph->min_x / w;
				float v0 = (float)glyph->min_y / h;
				float u1 = (float)glyph->max_x / w;
				float v1 = (float)glyph->max_y / h;

				vertex_immediate_t q0 = {
					.pos = { point.x, point.y, 0.0f },
					.tex = { u0, v1 },
					.col = color_packed,
				};

				vertex_immediate_t q1 = {
					.pos = { point.x + cw, point.y, 0.0f },
					.tex = { u1, v1 },
					.col = color_packed,
				};

				vertex_immediate_t q2 = {
					.pos = { point.x + cw, point.y + ch, 0.0f },
					.tex = { u1, v0 },
					.col = color_packed,
				};

				vertex_immediate_t q3 = {
					.pos = { point.x, point.y + ch, 0.0f },
					.tex = { u0, v0 },
					.col = color_packed,
				};

				r_immediate_quad(q0, q1, q2, q3);
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

rect2_t ui_text_bounds(font_atlas_t *font, v2_t p, string_t text)
{
	return ui_text_op(font, p, text, (v4_t){0,0,0,0}, UI_TEXT_OP_BOUNDS);
}

float ui_text_width(font_atlas_t *font, string_t text)
{
	rect2_t rect = ui_text_bounds(font, (v2_t){0, 0}, text);
	return rect2_width(rect);
}

float ui_text_height(font_atlas_t *font, string_t text)
{
	rect2_t rect = ui_text_bounds(font, (v2_t){0, 0}, text);
	return rect2_height(rect);
}

v2_t ui_text_dim(font_atlas_t *font, string_t text)
{
	rect2_t rect = ui_text_bounds(font, (v2_t){0, 0}, text);
	return rect2_dim(rect);
}

rect2_t ui_draw_text(font_atlas_t *font, v2_t p, string_t text)
{
	ui_text_op(font, add(p, make_v2(1.0f, -1.0f)), text, ui_color(UI_COLOR_TEXT_SHADOW), UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(font, p, text, ui_color(UI_COLOR_TEXT), UI_TEXT_OP_DRAW);
	return result;
}

rect2_t ui_draw_text_aligned(font_atlas_t *font, rect2_t rect, string_t text, v2_t align)
{
	v2_t p = ui_text_align_p(font, rect, text, align);
	ui_text_op(font, add(p, make_v2(1.0f, -1.0f)), text, ui_color(UI_COLOR_TEXT_SHADOW), UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(font, p, text, ui_color(UI_COLOR_TEXT), UI_TEXT_OP_DRAW);
	return result;
}

void ui_draw_rect(rect2_t rect, v4_t color)
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

void ui_draw_rect_roundedness(rect2_t rect, v4_t color, v4_t roundness)
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

void ui_draw_rect_roundedness_shadow(rect2_t rect, v4_t color, v4_t roundness, float shadow_amount, float shadow_radius)
{
	uint32_t color_packed = pack_color(color);

	r_ui_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = mul(roundness, v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS))),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
		.shadow_amount = shadow_amount,
		.shadow_radius = shadow_radius,
	});
}

void ui_draw_rect_outline(rect2_t rect, v4_t color, float width)
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

void ui_draw_rect_roundedness_outline(rect2_t rect, v4_t color, v4_t roundedness, float width)
{
	uint32_t color_packed = pack_color(color);

	r_ui_rect((r_ui_rect_t){
		.rect         = rect, 
		.roundedness  = mul(roundedness, v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS))),
		.color_00     = color_packed,
		.color_10     = color_packed,
		.color_11     = color_packed,
		.color_01     = color_packed,
		.inner_radius = width,
	});
}

void ui_draw_circle(v2_t p, float radius, v4_t color)
{
	uint32_t color_packed = pack_color(color);

	rect2_t rect = rect2_center_radius(p, make_v2(radius, radius));

	r_ui_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = v4_from_scalar(radius),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
	});
}

//
// Widget Building Utilities
//

bool ui_hover_rect(rect2_t rect)
{
	return rect2_contains_point(rect, ui.input.mouse_p);
}

ui_interaction_t ui_widget_behaviour(ui_id_t id, rect2_t rect)
{
	uint32_t result = 0;

	rect2_t hit_rect = rect;
	hit_rect.min.x = floorf(hit_rect.min.x);
	hit_rect.min.y = floorf(hit_rect.min.y);
	hit_rect.max.x = ceilf(hit_rect.max.x);
	hit_rect.max.y = ceilf(hit_rect.max.y);

	if (ui.panels.current_panel)
		hit_rect = rect2_intersect(ui.panels.current_panel->rect_init, rect);

	if (ui_is_active(id))
	{
		result |= UI_HELD;

		if (ui_mouse_buttons_released(PLATFORM_MOUSE_BUTTON_LEFT))
		{
			if (rect2_contains_point(hit_rect, ui.input.mouse_p))
			{
				result |= UI_FIRED;
			}

			result |= UI_RELEASED;

			ui_clear_active();
		}
	}
	else if (rect2_contains_point(hit_rect, ui.input.mouse_p))
	{
        ui_set_next_hot(id);
	}

	if (ui_is_hot(id) && ui_mouse_buttons_pressed(PLATFORM_MOUSE_BUTTON_LEFT))
	{
		result |= UI_PRESSED;
		ui.drag_anchor = sub(ui.input.mouse_p, rect2_center(rect));
		ui_set_active(id);
	}

	return result;
}

v2_t ui_text_align_p(font_atlas_t *font, rect2_t rect, string_t text, v2_t align)
{
    float text_width  = ui_text_width(font, text);
    float text_height = 0.5f*font->y_advance;

    float rect_width  = rect2_width(rect);
    float rect_height = rect2_height(rect);

    v2_t result = {
        .x = rect.min.x + align.x*rect_width  - align.x*text_width,
        .y = rect.min.y + align.y*rect_height - align.y*text_height,
    };

    return result;
}

v2_t ui_text_center_p(font_atlas_t *font, rect2_t rect, string_t text)
{
	return ui_text_align_p(font, rect, text, make_v2(0.5f, 0.5f));
}

v2_t ui_text_default_align_p(font_atlas_t *font, rect2_t rect, string_t text)
{
	return ui_text_align_p(font, rect, text, make_v2(ui_scalar(UI_SCALAR_TEXT_ALIGN_X), ui_scalar(UI_SCALAR_TEXT_ALIGN_Y)));
}

//
// Base Widgets
//

void ui_panel_begin(rect2_t rect)
{
	ui_panel_begin_ex(UI_ID_NULL, rect, 0);
}

void ui_panel_begin_ex(ui_id_t id, rect2_t rect, ui_panel_flags_t flags)
{
	ASSERT(ui.initialized);

	if (id.value) ui_push_id(id);

	r_push_view_screenspace_clip_rect(rect);

	float offset_y = 0.0f;

	if (flags & UI_PANEL_SCROLLABLE_VERT)
	{
		// rect2_t scroll_area = rect2_cut_right(&rect, ui_scalar(UI_SCALAR_SCROLLBAR_WIDTH));

		ui_panel_state_t *state = &ui_get_state(id)->panel;
		
		if (state->scrollable_height_y > 0.0f)
		{
			if (ui_hover_rect(rect))
			{
				state->scroll_offset_y -= ui.input.mouse_wheel;
				state->scroll_offset_y = flt_clamp(state->scroll_offset_y, 0.0f, max(0.0f, state->scrollable_height_y - rect2_height(rect)));
			}

			ui_id_t scrollbar_id = ui_child_id(S("scrollbar"), id);
#if 0

			float scroll_area_height = rect2_height(scroll_area);

			float visible_area_ratio = min(1.0f, rect2_height(rect) / state->scrollable_height_y);
			float handle_size      = visible_area_ratio*scroll_area_height;
			float handle_half_size = 0.5f*handle_size;

			float pct = state->scroll_offset_y / max(0.0f, (state->scrollable_height_y - rect2_height(rect)));

			if (ui_is_active(scrollbar_id))
			{
				float relative_y = CLAMP((ui.input.mouse_p.y - ui.drag_anchor.y) - scroll_area.min.y - handle_half_size, 0.0f, scroll_area_height - handle_size);
				pct = 1.0f - (relative_y / (scroll_area_height - handle_size));
			}

			// hmm
			pct = ui_interpolate_f32(ui_child_id(S("jejff"), scrollbar_id), pct);

			// state->scroll_offset_y = pct*(state->scrollable_height_y - rect2_height(rect));

			float height_exclusive = scroll_area_height - handle_size;
			float handle_offset = pct*height_exclusive;

			rect2_t top    = rect2_cut_top(&scroll_area, handle_offset);
			rect2_t handle = rect2_cut_top(&scroll_area, handle_size);
			rect2_t bot    = scroll_area;

			ui_widget_behaviour(scrollbar_id, handle);

			v4_t color = ui_color(UI_COLOR_SLIDER_FOREGROUND);

			ui_draw_rect(top, ui_color(UI_COLOR_SLIDER_BACKGROUND));
			ui_draw_rect(handle, color);
			ui_draw_rect(bot, ui_color(UI_COLOR_SLIDER_BACKGROUND));
#endif

			offset_y = roundf(ui_interpolate_f32(scrollbar_id, state->scroll_offset_y));
		}
	}

	rect2_t inner_rect = rect2_add_offset(rect, make_v2(0, offset_y));

	if (flags & UI_PANEL_SCROLLABLE_VERT)
	{
		inner_rect.min.y = inner_rect.max.y;
	}

	ui_panel_t *panel = ui_push_panel(id, inner_rect, flags);
	panel->rect_init = rect;
}

void ui_panel_end(void)
{
	ASSERT(ui.initialized);

	ui_panel_t *panel = ui.panels.current_panel;

	if (ALWAYS(panel))
	{
		ui_state_t *state = ui_get_state(panel->id);

		if (panel->flags & UI_PANEL_SCROLLABLE_VERT)
		{
			state->panel.scrollable_height_y = abs_ss(panel->rect_layout.max.y - panel->rect_layout.min.y);
		}

		if (panel->id.value)
		{
			ui_pop_id();
		}
	}

	r_pop_view();
	ui_pop_panel();
}

void ui_seperator(void)
{
	if (NEVER(!ui.initialized)) 
		return;

	rect2_t *layout = ui_layout_rect();

	float margin = 2.0f*ui_font_height();
	rect2_t rect = rect2_cut_top(layout, margin);
	float w = rect2_width(rect);
	float h = 2.0f*ui_scalar(UI_SCALAR_WIDGET_MARGIN);

	rect2_t seperator = rect2_center_dim(rect2_center(rect), make_v2(w, h));
	ui_draw_rect(seperator, make_v4(0.1f, 0.1f, 0.1f, 0.25f));
}

void ui_label(string_t label)
{
	if (NEVER(!ui.initialized)) 
		return;

	rect2_t rect = ui_default_label_rect(&ui.style.font, label);
	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));
	ui_draw_text(&ui.style.font, ui_text_default_align_p(&ui.style.font, text_rect, label), label);
}

void ui_header(string_t label)
{
	if (NEVER(!ui.initialized)) 
		return;

	rect2_t rect = ui_default_label_rect(&ui.style.header_font, label);
	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));
	ui_draw_text(&ui.style.header_font, ui_text_default_align_p(&ui.style.header_font, text_rect, label), label);
}

float ui_label_width(string_t label)
{
    return ui_text_width(&ui.style.font, label) + 2.0f*ui_widget_padding();
}

void ui_progress_bar(string_t label, float progress)
{
	if (NEVER(!ui.initialized)) 
		return;

	rect2_t rect = ui_default_label_rect(&ui.style.font, label);
	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	// TODO: Unhardcore direction

	float width = rect2_width(rect);

	rect2_t tray   = rect;
	rect2_t filled = rect2_cut_left(&rect, progress*width);

	ui_draw_rect(tray, ui_color(UI_COLOR_PROGRESS_BAR_EMPTY));
	ui_draw_rect(filled, ui_color(UI_COLOR_PROGRESS_BAR_FILLED));

	ui_draw_text(&ui.style.font, ui_text_default_align_p(&ui.style.font, text_rect, label), label);
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
	ui_id_t color_id = ui_child_id(S("color"), id);

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
		INVALID_DEFAULT_CASE;
	}

	float stiffness = ui_scalar(UI_SCALAR_ANIMATION_STIFFNESS);
	if (state == UI_WIDGET_STATE_ACTIVE)
	{
		stiffness *= 2.0f;
	}

	v4_t interp_color;

	UI_SCALAR(UI_SCALAR_ANIMATION_STIFFNESS, stiffness)
	interp_color = ui_interpolate_v4(color_id, target);

	return interp_color;
}

bool ui_button(string_t label)
{
	bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	ui_id_t id = ui_id(label);

	rect2_t rect = ui_default_label_rect(&ui.style.font, label);
	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t text_rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	uint32_t interaction = ui_widget_behaviour(id, rect);
	result = interaction & UI_FIRED;

	ui_widget_state_t state = ui_get_widget_state(id);

	v4_t color = ui_animate_colors(id, state, interaction, 
								   ui_color(UI_COLOR_BUTTON_IDLE),
								   ui_color(UI_COLOR_BUTTON_HOT),
								   ui_color(UI_COLOR_BUTTON_ACTIVE),
								   ui_color(UI_COLOR_BUTTON_FIRED));

	float hover_lift = ui_is_hot(id) && !ui_is_active(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(S("hover_lift"), id), hover_lift);

	rect2_t shadow = rect;

	rect      = rect2_add_offset(rect,      make_v2(0.0f, hover_lift));
	text_rect = rect2_add_offset(text_rect, make_v2(0.0f, hover_lift));

	ui_draw_rect(shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_rect(rect, color);

	ui_draw_text(&ui.style.font, ui_text_center_p(&ui.style.font, text_rect, label), label);

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

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();

		float a = ui_widget_padding();

		switch (panel->layout_direction)
		{
			case RECT2_CUT_LEFT:
			case RECT2_CUT_RIGHT:
			{
				a += ui_text_width(&ui.style.font, label) + ui_font_height() + ui_widget_padding();
			} break;

			case RECT2_CUT_TOP:
			case RECT2_CUT_BOTTOM:
			{
				a += ui_font_height();
			} break;
		}

		rect = rect2_do_cut((rect2_cut_t){ .side = panel->layout_direction, .rect = &panel->rect_layout }, a);
	}

	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t outer_box_rect = rect2_cut_left(&rect, rect2_height(rect));

	float outer_rect_rel_width = 0.15f;
	float outer_rect_c = outer_rect_rel_width*min(rect2_width(outer_box_rect), rect2_height(outer_box_rect));

	rect2_t inner_box_rect = rect2_shrink(outer_box_rect, outer_rect_c*1.5f);
	rect2_t label_rect = rect2_cut_left(&rect, ui_text_width(&ui.style.font, label));
	label_rect = rect2_shrink(label_rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	rect2_t full_widget_rect = rect2_union(label_rect, outer_box_rect);

	uint32_t interaction = ui_widget_behaviour(id, full_widget_rect);
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
	hover_lift = ui_interpolate_f32(ui_child_id(S("hover_lift"), id), hover_lift);

	// clamp roundedness to avoid the checkbox looking like a radio button and
	// transfer roundedness of outer rect to inner rect so the countours match
	float roundedness_ratio = min(0.66f, ui_roundedness_ratio(outer_box_rect));
	float outer_roundedness = ui_roundedness_ratio_to_abs(outer_box_rect, roundedness_ratio);
	float inner_roundedness = ui_roundedness_ratio_to_abs(inner_box_rect, roundedness_ratio);

	UI_SCALAR(UI_SCALAR_ROUNDEDNESS, outer_roundedness)
	{
		rect2_t outer_box_rect_shadow = outer_box_rect;
		outer_box_rect = rect2_add_offset(outer_box_rect, make_v2(0, hover_lift));

		ui_draw_rect_outline(outer_box_rect_shadow, ui_color(UI_COLOR_WIDGET_SHADOW), outer_rect_c);
		ui_draw_rect_outline(outer_box_rect, color, outer_rect_c);
	}

	if (*value) 
	{
		UI_SCALAR(UI_SCALAR_ROUNDEDNESS, inner_roundedness)
		{
			rect2_t inner_box_rect_shadow = inner_box_rect;
			inner_box_rect = rect2_add_offset(inner_box_rect, make_v2(0, hover_lift));

			ui_draw_rect(inner_box_rect_shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
			ui_draw_rect(inner_box_rect, color);
		}
	}

	float x = label_rect.min.x;
	float y = ui_text_center_p(&ui.style.font, label_rect, label).y;
	v2_t  p = make_v2(x, y + hover_lift);

	ui_draw_text(&ui.style.font, p, label);

	return result;
}

bool ui_option_buttons(string_t label, int *value, int count, string_t *labels)
{
    bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	if (NEVER(!value))
		return result;

	if (NEVER(!labels || count == 0))
		return result;

	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = rect2_cut_top(&panel->rect_layout, ui_font_height() + ui_widget_padding());
	}

	rect2_t label_rect = rect2_cut_left(&rect, ui_text_width(&ui.style.font, label) + ui_widget_padding());
	label_rect = rect2_shrink(label_rect, 0.5f*ui_widget_padding());

    {
        float size = rect2_width(rect) / (float)count;
        for (int i = 0; i < count; i++)
        {
            bool selected = *value == i;

            if (selected)
            {
                ui_push_color(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_BUTTON_ACTIVE));
            }

            ui_set_next_rect(rect2_cut_left(&rect, size));
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

	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

    return result;
}

bool ui_slider(string_t label, float *v, float min, float max)
{
	if (NEVER(!ui.initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	float init_v = *v;

	ui_id_t id = ui_id_pointer(v);

	// TODO: Handle layout direction properly
	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = rect2_cut_top(&panel->rect_layout, ui_font_height() + ui_widget_padding());
	}

	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	float w = rect2_width(rect);

	rect2_t label_rect = rect2_cut_left(&rect, 0.5f*w); // (float)label.count*ui.font.cw + 2.0f*ui_scalar(UI_SCALAR_TEXT_MARGIN));
	label_rect = rect2_shrink(label_rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	rect2_t slider_body = rect;

	float handle_width = max(16.0f, rect2_width(rect)*ui_scalar(UI_SCALAR_SLIDER_HANDLE_RATIO));
	float handle_half_width = 0.5f*handle_width;
	
	float width = rect2_width(rect);
	float relative_x = CLAMP((ui.input.mouse_p.x - ui.drag_anchor.x) - rect.min.x - handle_half_width, 0.0f, width - handle_width);

	float pct = 0.0f;

	if (ui_is_active(id))
	{
		pct = relative_x / (width - handle_width);
		*v = min + pct*(max - min);
		ui_set_f32(id, pct);
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

	rect2_t left   = rect2_cut_left(&rect, handle_offset);
	rect2_t handle = rect2_cut_left(&rect, handle_width);
	rect2_t right  = rect;

	(void)left;
	(void)right;

	uint32_t interaction = ui_widget_behaviour(id, handle);
	ui_widget_state_t state = ui_get_widget_state(id);

	v4_t color = ui_animate_colors(id, state, interaction, 
								   ui_color(UI_COLOR_SLIDER_FOREGROUND),
								   ui_color(UI_COLOR_SLIDER_HOT),
								   ui_color(UI_COLOR_SLIDER_ACTIVE),
								   ui_color(UI_COLOR_SLIDER_ACTIVE));

	float hover_lift = ui_is_hot(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(S("hover_lift"), id), hover_lift);

	rect2_t shadow = handle;
	handle = rect2_add_offset(handle, make_v2(0, hover_lift));

	// r_immediate_clip_rect(slider_body); TODO: ui rect clip rects?
	ui_draw_rect(slider_body, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	ui_draw_rect(shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_rect(handle, color);
	ui_draw_rect(right, ui_color(UI_COLOR_SLIDER_BACKGROUND));

	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

	string_t value_text = Sf("%.03f", *v);

	v2_t text_p = ui_text_center_p(&ui.style.font, slider_body, value_text);
	ui_draw_text(&ui.style.font, text_p, value_text);

	return *v != init_v;
}

bool ui_slider_int(string_t label, int *v, int min, int max)
{
	if (NEVER(!ui.initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	int init_value = *v;

	ui_id_t id = ui_id_pointer(v);

	// TODO: Handle layout direction properly
	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = rect2_cut_top(&panel->rect_layout, ui_font_height() + ui_widget_padding());
	}

	float w = rect2_width(rect);

	rect2_t label_rect = rect2_cut_left(&rect, 0.5f*w); // (float)label.count*ui.font.cw + ui_widget_padding());
	label_rect = rect2_shrink(label_rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN) + ui_scalar(UI_SCALAR_TEXT_MARGIN));

	rect2_t sub_rect = rect2_cut_left (&rect, rect2_height(rect));
	rect2_t add_rect = rect2_cut_right(&rect, rect2_height(rect));

	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	ui_id_t sub_id = ui_child_id(S("sub"), id);
	ui_set_next_id  (sub_id);
	ui_set_next_rect(sub_rect);
	if (ui_button(S("-")))
	{
		if (*v > min) *v = *v - 1;
	}

	ui_id_t add_id = ui_child_id(S("add"), id);
	ui_set_next_id  (add_id);
	ui_set_next_rect(add_rect);
	if (ui_button(S("+")))
	{
		if (*v < max) *v = *v + 1;
	}

	rect2_t slider_body = rect;

	int   range = max - min;
	float ratio = 1.0f / (float)range;

	float handle_width = max(16.0f, rect2_width(rect)*ratio); // ui_scalar(UI_SCALAR_SLIDER_HANDLE_RATIO);
	float handle_half_width = 0.5f*handle_width;
	
	float width = rect2_width(rect);
	float relative_x = CLAMP((ui.input.mouse_p.x - ui.drag_anchor.x) - rect.min.x - handle_half_width, 0.0f, width - handle_width);

	float pct = 0.0f;

	if (ui_is_active(id))
	{
		pct = relative_x / (width - handle_width);
		*v = (int)roundf((float)min + pct*(float)(range));
		ui_set_f32(id, pct);
	}
	else
	{
		int value = *v;
		pct = (float)(value - min) / (float)(range); // TODO: protect against division by zero, think about min > max case
	}

	pct = saturate(pct);

	float hover_lift = ui_is_hot(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(S("hover_lift"), id), hover_lift);

	UI_SCALAR(UI_SCALAR_ANIMATION_STIFFNESS, 4.0f*ui_scalar(UI_SCALAR_ANIMATION_STIFFNESS));
	float pct_interp = ui_interpolate_f32(id, pct);

	float width_exclusive = width - handle_width;
	float handle_offset = pct_interp*width_exclusive;

	rect2_t left   = rect2_cut_left(&rect, handle_offset);
	rect2_t handle = rect2_cut_left(&rect, handle_width);
	rect2_t right  = rect;

	uint32_t interaction = ui_widget_behaviour(id, handle);

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

	ui_draw_rect(slider_body, ui_color(UI_COLOR_SLIDER_BACKGROUND));

	ui_draw_rect(handle_no_offset, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_rect(handle, color);

	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

	string_t value_text = Sf("%d", *v);

	v2_t text_p = ui_text_center_p(&ui.style.font, slider_body, value_text);
	ui_draw_text(&ui.style.font, text_p, value_text);

	return *v != init_value;
}

//
// Core
//

ui_t ui = {
	.state = INIT_POOL(ui_state_t),

	.style = {
		.animation_state = INIT_POOL(ui_anim_t),
	},
};

bool ui_is_cold(ui_id_t id)
{
	return (ui.hot.value != id.value && ui.active.value != id.value);
}

bool ui_is_next_hot(ui_id_t id)
{
	return ui.next_hot.value == id.value;
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

void ui_clear_next_hot(void)
{
	ui.next_hot.value = 0;
}

void ui_clear_hot(void)
{
	ui.hot.value = 0;
}

void ui_clear_active(void)
{
	ui.active.value = 0;
}

ui_state_t *ui_get_state(ui_id_t id)
{
	ui_state_t *result = hash_find_object(&ui.state_index, id.value);

	if (!result)
	{
		result = pool_add(&ui.state);
		result->id                  = id;
		result->created_frame_index = ui.frame_index;
		hash_add_object(&ui.state_index, id.value, result);
	}

	ASSERT(result->id.value == id.value);

	result->last_touched_frame_index = ui.frame_index;
	return result;
}

bool ui_state_is_new(ui_state_t *state)
{
	return state->created_frame_index == ui.frame_index;
}

bool ui_begin(float dt)
{
	if (!ui.initialized)
	{
		ui.style.font_data        = fs_read_entire_file(&ui.arena, S("gamedata/fonts/NotoSans/NotoSans-Regular.ttf"));
		ui.style.header_font_data = fs_read_entire_file(&ui.arena, S("gamedata/fonts/NotoSans/NotoSans-Bold.ttf"));
		ui_set_font_height(18.0f);

		v4_t background    = make_v4(0.20f, 0.15f, 0.17f, 1.0f);
		v4_t background_hi = make_v4(0.22f, 0.18f, 0.18f, 1.0f);
		v4_t foreground    = make_v4(0.33f, 0.28f, 0.28f, 1.0f);

		v4_t hot    = make_v4(0.25f, 0.45f, 0.40f, 1.0f);
		v4_t active = make_v4(0.30f, 0.55f, 0.50f, 1.0f);
		v4_t fired  = make_v4(0.45f, 0.65f, 0.55f, 1.0f);

        ui.style.base_scalars[UI_SCALAR_ANIMATION_RATE     ] = 40.0f;
        ui.style.base_scalars[UI_SCALAR_ANIMATION_STIFFNESS] = 512.0f;
        ui.style.base_scalars[UI_SCALAR_ANIMATION_DAMPEN   ] = 32.0f;
        ui.style.base_scalars[UI_SCALAR_HOVER_LIFT         ] = 1.5f;
		ui.style.base_scalars[UI_SCALAR_WINDOW_MARGIN      ] = 0.0f;
		ui.style.base_scalars[UI_SCALAR_WIDGET_MARGIN      ] = 0.75f;
		ui.style.base_scalars[UI_SCALAR_TEXT_MARGIN        ] = 2.2f;
		ui.style.base_scalars[UI_SCALAR_ROUNDEDNESS        ] = 2.5f;
		ui.style.base_scalars[UI_SCALAR_TEXT_ALIGN_X       ] = 0.5f;
		ui.style.base_scalars[UI_SCALAR_TEXT_ALIGN_Y       ] = 0.5f;
		ui.style.base_scalars[UI_SCALAR_SCROLLBAR_WIDTH    ] = 12.0f;
        ui.style.base_scalars[UI_SCALAR_SLIDER_HANDLE_RATIO] = 1.0f / 4.0f;
		ui.style.base_colors [UI_COLOR_TEXT                ] = make_v4(0.95f, 0.90f, 0.85f, 1.0f);
		ui.style.base_colors [UI_COLOR_TEXT_SHADOW         ] = make_v4(0.00f, 0.00f, 0.00f, 0.50f);
		ui.style.base_colors [UI_COLOR_WIDGET_SHADOW       ] = make_v4(0.00f, 0.00f, 0.00f, 0.20f);
		ui.style.base_colors [UI_COLOR_WINDOW_BACKGROUND   ] = background;
		ui.style.base_colors [UI_COLOR_WINDOW_TITLE_BAR    ] = make_v4(0.45f, 0.25f, 0.25f, 1.0f);
		ui.style.base_colors [UI_COLOR_WINDOW_TITLE_BAR_HOT] = make_v4(0.45f, 0.22f, 0.22f, 1.0f);
		ui.style.base_colors [UI_COLOR_WINDOW_CLOSE_BUTTON ] = make_v4(0.35f, 0.15f, 0.15f, 1.0f);
		ui.style.base_colors [UI_COLOR_WINDOW_OUTLINE      ] = make_v4(0.1f, 0.1f, 0.1f, 0.20f);
		ui.style.base_colors [UI_COLOR_PROGRESS_BAR_EMPTY  ] = background_hi;
		ui.style.base_colors [UI_COLOR_PROGRESS_BAR_FILLED ] = hot;
		ui.style.base_colors [UI_COLOR_BUTTON_IDLE         ] = foreground;
		ui.style.base_colors [UI_COLOR_BUTTON_HOT          ] = hot;
		ui.style.base_colors [UI_COLOR_BUTTON_ACTIVE       ] = active;
		ui.style.base_colors [UI_COLOR_BUTTON_FIRED        ] = fired;
		ui.style.base_colors [UI_COLOR_SLIDER_BACKGROUND   ] = background_hi;
		ui.style.base_colors [UI_COLOR_SLIDER_FOREGROUND   ] = foreground;
		ui.style.base_colors [UI_COLOR_SLIDER_HOT          ] = hot;
		ui.style.base_colors [UI_COLOR_SLIDER_ACTIVE       ] = active;

		ui.initialized = true;
	}

	ui.frame_index += 1;
	ui.input.dt = dt;

	for (pool_iter_t it = pool_iter(&ui.state); 
		 pool_iter_valid(&it); 
		 pool_iter_next(&it))
	{
		ui_state_t *state = it.data;

		if (state->last_touched_frame_index + 1 < ui.frame_index)
		{
			hash_rem     (&ui.state_index, state->id.value);
			pool_rem_item(&ui.state, state);
		}
	}

	for (pool_iter_t it = pool_iter(&ui.style.animation_state); 
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

		t_velocity = add(t_velocity, mul(ui.input.dt, accel));
		t_current  = add(t_current,  mul(ui.input.dt, t_velocity));

		anim->t_current  = t_current;
		anim->t_velocity = t_velocity;
	}

    if (!ui.active.value)
        ui.hot = ui.next_hot;

	ui.next_hot = UI_ID_NULL;

	ui.hovered = false;
	
	if (ui_mouse_buttons_pressed(PLATFORM_MOUSE_BUTTON_LEFT))
	{
		ui.input.mouse_pressed_p = ui.input.mouse_p;
	}

	if (ui.input.cursor_reset_delay > 0)
	{
		ui.input.cursor_reset_delay -= 1;
	}
	else
	{
		ui.input.cursor = PLATFORM_CURSOR_ARROW;
	}

	return ui.has_focus;
}

void ui_end(void)
{
	if (ui_mouse_buttons_pressed(PLATFORM_MOUSE_BUTTON_ANY))
	{
		ui.has_focus = ui.hovered;
	}

	ui.input.mouse_wheel            = 0.0f;
	ui.input.mouse_buttons_pressed  = 0;
	ui.input.mouse_buttons_released = 0;

	ASSERT(ui.panels.current_panel == NULL);
	ASSERT(ui.id_stack.at == 0);
}
