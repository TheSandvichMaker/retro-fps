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

	return ui_child_id(parent, string);
}

ui_id_t ui_child_id(ui_id_t parent, string_t string)
{
	ui_id_t result = {0};

	if (ui.next_id.value)
	{
		result = ui.next_id;
		ui.next_id = UI_ID_NULL;
	}
	else
	{
		uint64_t hash = string_hash_with_seed(string, parent.value);

		if (hash == 0)
			hash = ~(uint64_t)0;

		result.value = hash;

#if DREAM_SLOW
		string_storage_append(&result.name, string);
#endif
	}

	return result;
}

ui_id_t ui_id_pointer(void *pointer)
{
	ui_id_t result = { (uint64_t)(uintptr_t)pointer };

	if (ui.next_id.value)
	{
		result = ui.next_id;
		ui.next_id = UI_ID_NULL;
	}

	return result;
}

ui_id_t ui_id_u64(uint64_t u64)
{
	ui_id_t result = { u64 };

	if (ui.next_id.value)
	{
		result = ui.next_id;
		ui.next_id = UI_ID_NULL;
	}

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

void ui_submit_text(string_t text)
{
	dyn_string_appends(&ui.input.text, text);
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

void ui_open_popup(ui_id_t id)
{
	bool popup_exists = false;

	for (size_t i = 0; i < ui.popup_stack.at; i++)
	{
		if (ui.popup_stack.values[i].id.value == id.value)
		{
			popup_exists = true;
		}
	}

	if (!popup_exists)
	{
		ui_popup_t popup = {
			.id = id, 
		};

		stack_push(ui.popup_stack, popup);
	}
}

void ui_close_popup(ui_id_t id)
{
	for (size_t i = 0; i < ui.popup_stack.at; i++)
	{
		if (ui.popup_stack.values[i].id.value == id.value)
		{
			stack_remove(ui.popup_stack, i);
			continue;
		}
	}
}

bool ui_popup_is_open(ui_id_t id)
{
	bool result = false;

	for (size_t i = 0; i < ui.popup_stack.at; i++)
	{
		if (ui.popup_stack.values[i].id.value == id.value)
		{
			result = true;
			break;
		}
	}

	return result;
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

void ui_open_window(ui_window_t *window)
{
	window->open = true;
	ui_bring_window_to_front(window);
	ui_focus_window         (window);
}

void ui_close_window(ui_window_t *window)
{
	window->open = false;
	ui_send_window_to_back(window);
	if (ui.windows.focus_window == window && ui.windows.last_window != window)
	{
		ui_focus_window(ui.windows.last_window);
	}
}

void ui_toggle_window_openness(ui_window_t *window)
{
	if (window->open)
	{
		ui_close_window(window);
	}
	else
	{
		ui_open_window(window);
	}
}

void ui_process_windows(void)
{
	ui_window_t *hovered_window = NULL;

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

			float tray_width = 8.0f;

			rect2_t interact_total = rect2_extend(total, 0.5f*tray_width);
			ui_interaction_t interaction = ui_default_widget_behaviour(id, interact_total);

			if (ui_mouse_in_rect(interact_total))
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
			tray_interaction |= ui_default_widget_behaviour(id_n,  tray_n);
			tray_interaction |= ui_default_widget_behaviour(id_e,  tray_e);
			tray_interaction |= ui_default_widget_behaviour(id_s,  tray_s);
			tray_interaction |= ui_default_widget_behaviour(id_w,  tray_w);
			tray_interaction |= ui_default_widget_behaviour(id_ne, tray_ne);
			tray_interaction |= ui_default_widget_behaviour(id_nw, tray_nw);
			tray_interaction |= ui_default_widget_behaviour(id_se, tray_se);
			tray_interaction |= ui_default_widget_behaviour(id_sw, tray_sw);

			if (tray_interaction & UI_PRESSED)
			{
				ui.resize_original_rect = window->rect;
			}

			// draw window

			rect2_t title_bar_minus_outline = title_bar;
			title_bar_minus_outline.max.y -= 2.0f;

			string_t title = string_from_storage(window->title);

			bool has_focus = ui_has_focus() && (window == ui.windows.focus_window);

			float focus_t = ui_interpolate_f32(ui_id(S("focus")), has_focus);
			float shadow_amount = lerp(0.15f, 0.25f, focus_t);

			v4_t  title_bar_color           = ui_color(UI_COLOR_WINDOW_TITLE_BAR);
			float title_bar_luma            = luminance(title_bar_color.xyz);
			v4_t  title_bar_color_greyscale = make_v4(title_bar_luma, title_bar_luma, title_bar_luma, 1.0f);

			v4_t  interpolated_title_bar_color = v4_lerps(title_bar_color_greyscale, title_bar_color, focus_t);

			ui_draw_rect_roundedness_shadow(rect2_shrink(total, 1.0f), make_v4(0, 0, 0, 0), make_v4(5, 5, 5, 5), shadow_amount, 32.0f);
			ui_draw_rect_roundedness(title_bar, interpolated_title_bar_color, make_v4(2, 0, 2, 0));
			ui_draw_rect_roundedness(rect, ui_color(UI_COLOR_WINDOW_BACKGROUND), make_v4(0, 2, 0, 2));
			ui_draw_text(&ui.style.header_font, ui_text_center_p(&ui.style.header_font, title_bar_minus_outline, title), title);
			ui_draw_rect_roundedness_outline(total, ui_color(UI_COLOR_WINDOW_OUTLINE), make_v4(2, 2, 2, 2), 2.0f);

			// handle window contents

			float margin = max(ui_scalar(UI_SCALAR_WIDGET_MARGIN), 0.5f*ui_scalar(UI_SCALAR_ROUNDEDNESS));
			rect = rect2_shrink(rect, margin);
			rect = rect2_pillarbox(rect, ui_scalar(UI_SCALAR_WINDOW_MARGIN));

			// TODO: Custom scrollbar rendering for windows?
			ui_panel_begin_ex(ui_id(S("panel")), rect, UI_PANEL_SCROLLABLE_VERT);

			ui_push_clip_rect(rect);

			window->focused = (window == ui.windows.last_window);
			window->draw_proc(window);

			ui_pop_clip_rect();

			ui_panel_end();

			window->hovered = false;
		}
		ui_pop_id();
	}

	if (hovered_window)
	{
		ui.hovered = true;
		hovered_window->hovered = true;
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
	ui_anim_t *result = table_find_object(&ui.style.animation_index, id.value);

	if (!result)
	{
		result = pool_add(&ui.style.animation_state);
		result->id        = id;
		result->t_target  = init_value;
		result->t_current = init_value;
		table_insert_object(&ui.style.animation_index, id.value, result);
	}

	ASSERT(result->id.value == id.value);

	result->length_limit = ui_scalar(UI_SCALAR_ANIMATION_LENGTH_LIMIT);
	result->c_t = ui_scalar(UI_SCALAR_ANIMATION_STIFFNESS);
	result->c_v = ui_scalar(UI_SCALAR_ANIMATION_DAMPEN);

	result->last_touched_frame_index = ui.frame_index;

	return result;
}

float ui_interpolate_f32_start(ui_id_t id, float target, float start)
{
	ui_anim_t *anim = ui_get_anim(id, v4_from_scalar(start));
	anim->t_target.x = target;

	return anim->t_current.x;
}

float ui_interpolate_f32(ui_id_t id, float target)
{
	return ui_interpolate_f32_start(id, target, target);
}

float ui_interpolate_snappy_f32(ui_id_t id, float target)
{
	ui_anim_t *anim = ui_get_anim(id, v4_from_scalar(target));
	anim->t_target.x = target;
	anim->c_t *= 2.0f;
	anim->c_v *= 2.0f;

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

void ui_push_clip_rect(rect2_t rect)
{
	if (ALWAYS(!stack_full(ui.clip_rect_stack)))
	{
		r_rect2_fixed_t fixed = rect2_to_fixed(rect);
		stack_push(ui.clip_rect_stack, fixed);
	}
}

void ui_pop_clip_rect(void)
{
	if (ALWAYS(!stack_empty(ui.clip_rect_stack)))
	{
		stack_pop(ui.clip_rect_stack);
	}
}

DREAM_INLINE r_rect2_fixed_t ui_get_clip_rect(void)
{
	r_rect2_fixed_t clip_rect = {
		0, 0, UINT16_MAX, UINT16_MAX,
	};

	if (!stack_empty(ui.clip_rect_stack))
	{
		clip_rect = stack_top(ui.clip_rect_stack);
	}

	return clip_rect;
}

rect2_t ui_text_op(font_atlas_t *font, v2_t p, string_t text, v4_t color, ui_text_op_t op)
{
	rect2_t result = rect2_inverted_infinity();

	uint32_t color_packed = pack_color(color);

	p.x = roundf(p.x);
	p.y = roundf(p.y);

	m_scoped_temp
	{
		prepared_glyphs_t prep = atlas_prepare_glyphs(font, temp, text);
		result = rect2_add_offset(prep.bounds, p);

		if (op == UI_TEXT_OP_DRAW)
		{
			for (size_t i = 0; i < prep.count; i++)
			{
				prepared_glyph_t *glyph = &prep.glyphs[i];

				rect2_t rect    = glyph->rect;
				rect2_t rect_uv = glyph->rect_uv;

				rect = rect2_add_offset(rect, p);

                ui_push_command(
					(ui_render_command_key_t){
						.layer  = ui.render_layer,
						.window = 0,
					},

					&(ui_render_command_t){
						.texture = font->texture,
						.rect = {
							.rect = rect,
							.tex_coords = rect_uv,
							.color_00 = color_packed,
							.color_10 = color_packed,
							.color_11 = color_packed,
							.color_01 = color_packed,
							.flags    = R_UI_RECT_BLEND_TEXT,
							.clip_rect = ui_get_clip_rect(),
						},
					}
                );
			}
		}
	}

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

v2_t ui_text_align_p(font_atlas_t *font, rect2_t rect, string_t text, v2_t align)
{
    float text_width  = ui_text_width(font, text);
    float text_height = font->y_advance;

    float rect_width  = rect2_width(rect);
    float rect_height = rect2_height(rect);

    v2_t result = {
        .x = rect.min.x + align.x*rect_width  - align.x*text_width,
        .y = rect.min.y + align.y*rect_height - align.y*text_height - font->descent,
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

rect2_t ui_draw_text(font_atlas_t *font, v2_t p, string_t text)
{
	v4_t text_color   = ui_color(UI_COLOR_TEXT);
	v4_t shadow_color = ui_color(UI_COLOR_TEXT_SHADOW);

	shadow_color.w *= text_color.w;

	ui_text_op(font, add(p, make_v2(1.0f, -1.0f)), text, shadow_color, UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(font, p, text, text_color, UI_TEXT_OP_DRAW);
	return result;
}

rect2_t ui_draw_text_aligned(font_atlas_t *font, rect2_t rect, string_t text, v2_t align)
{
	v2_t p = ui_text_align_p(font, rect, text, align);

	v4_t text_color   = ui_color(UI_COLOR_TEXT);
	v4_t shadow_color = ui_color(UI_COLOR_TEXT_SHADOW);

	shadow_color.w *= text_color.w;

	ui_text_op(font, add(p, make_v2(1.0f, -1.0f)), text, shadow_color, UI_TEXT_OP_DRAW);
	rect2_t result = ui_text_op(font, p, text, text_color, UI_TEXT_OP_DRAW);

	return result;
}

void ui_reset_render_commands(void)
{
	ui.render_commands.count = 0;
}

void ui_push_command(ui_render_command_key_t key, const ui_render_command_t *command)
{
	ui_render_command_list_t *list = &ui.render_commands;

	if (ALWAYS(list->count < list->capacity))
	{
		size_t index = list->count++;

		ASSERT(index <= UINT16_MAX);

		key.command_index = (uint16_t)index;

		list->keys    [index] = key;
		list->commands[index] = *command;
	}
}

void ui_sort_render_commands(void)
{
	radix_sort_u32(&ui.render_commands.keys[0].u32, ui.render_commands.count);
}

// TODO: remove... silly function
DREAM_INLINE void ui_do_rect(r_ui_rect_t rect)
{
	rect.clip_rect = ui_get_clip_rect();

	ui_push_command(
		(ui_render_command_key_t){
			.layer  = ui.render_layer,
			.window = 0,
		},
		&(ui_render_command_t){
			.rect = rect,
		}
	);
}

void ui_draw_rect(rect2_t rect, v4_t color)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect        = rect, 
		.roundedness = v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS)),
		.color_00    = color_packed,
		.color_10    = color_packed,
		.color_11    = color_packed,
		.color_01    = color_packed,
	});
}

void ui_draw_rect_shadow(rect2_t rect, v4_t color, float shadow_amount, float shadow_radius)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
		.rect          = rect, 
		.roundedness   = v4_from_scalar(ui_scalar(UI_SCALAR_ROUNDEDNESS)),
		.color_00      = color_packed,
		.color_10      = color_packed,
		.color_11      = color_packed,
		.color_01      = color_packed,
		.shadow_amount = shadow_amount,
		.shadow_radius = shadow_radius,
	});
}

void ui_draw_rect_roundedness(rect2_t rect, v4_t color, v4_t roundness)
{
	uint32_t color_packed = pack_color(color);

	ui_do_rect((r_ui_rect_t){
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

	ui_do_rect((r_ui_rect_t){
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

	ui_do_rect((r_ui_rect_t){
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

	ui_do_rect((r_ui_rect_t){
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

	ui_do_rect((r_ui_rect_t){
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

bool ui_mouse_in_rect(rect2_t rect)
{
	return rect2_contains_point(rect, ui.input.mouse_p);
}

ui_interaction_t ui_default_widget_behaviour_priority(ui_id_t id, rect2_t rect, ui_priority_t priority)
{
	uint32_t result = 0;

	rect2_t hit_rect = rect;
	hit_rect.min.x = floorf(hit_rect.min.x);
	hit_rect.min.y = floorf(hit_rect.min.y);
	hit_rect.max.x = ceilf(hit_rect.max.x);
	hit_rect.max.y = ceilf(hit_rect.max.y);

	if (ui.panels.current_panel)
		hit_rect = rect2_intersect(ui.panels.current_panel->rect_init, rect);

	bool hovered = rect2_contains_point(hit_rect, ui.input.mouse_p);

	if (hovered)
	{
        ui_set_next_hot(id, priority);
		result |= UI_HOVERED;
	}

	if (ui_is_active(id))
	{
		result |= UI_HELD;

		if (ui_mouse_buttons_released(PLATFORM_MOUSE_BUTTON_LEFT))
		{
			if (hovered)
			{
				result |= UI_FIRED;
			}

			result |= UI_RELEASED;

			ui_clear_active();
		}
	}

	if (ui_is_hot(id))
	{
		result |= UI_HOT;

		if (ui_mouse_buttons_pressed(PLATFORM_MOUSE_BUTTON_LEFT))
		{
			result |= UI_PRESSED;
			ui.drag_anchor = sub(ui.input.mouse_p, rect2_center(rect));
			ui_set_active(id);
		}
	}

	return result;
}

ui_interaction_t ui_default_widget_behaviour(ui_id_t id, rect2_t rect)
{
	ui_priority_t priority = UI_PRIORITY_DEFAULT;

	if (ui.override_priority != UI_PRIORITY_DEFAULT)
	{
		priority = ui.override_priority;
	}

	return ui_default_widget_behaviour_priority(id, rect, priority);
}

float ui_roundedness_ratio(rect2_t rect)
{
	float w = rect2_width (rect);
	float h = rect2_height(rect);
	float c = 0.5f*min(w, h);
	float ratio = ui_scalar(UI_SCALAR_ROUNDEDNESS) / c;
	return ratio;
}

float ui_roundedness_ratio_to_abs(rect2_t rect, float ratio)
{
	float w = rect2_width (rect);
	float h = rect2_height(rect);
	float c = 0.5f*min(w, h);
	float abs = c*ratio;
	return abs;
}

rect2_t ui_cut_widget_rect(v2_t min_size)
{
	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();

		bool layout_is_horz = (panel->layout_direction == RECT2_CUT_LEFT ||
							   panel->layout_direction == RECT2_CUT_RIGHT);

		float a = ui_widget_padding() + (layout_is_horz ? min_size.x : min_size.y);
		rect = rect2_do_cut((rect2_cut_t){ .side = panel->layout_direction, .rect = &panel->rect_layout }, a);
		rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));
	}
	return rect;
}

float ui_hover_lift(ui_id_t id)
{
	float hover_lift = ui_is_hot(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);
	return hover_lift;
}

float ui_button_style_hover_lift(ui_id_t id)
{
	float hover_lift = ui_is_hot(id) && !ui_is_active(id) ? ui_scalar(UI_SCALAR_HOVER_LIFT) : 0.0f;
	hover_lift = ui_interpolate_f32(ui_child_id(id, S("hover_lift")), hover_lift);
	return hover_lift;
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

	if (id.value) 
	{
		ui_push_id(id);

		if (ui_mouse_in_rect(rect))
		{
			ui.next_hovered_panel = id;
		}
	}

	float offset_y = 0.0f;

	if (flags & UI_PANEL_SCROLLABLE_VERT)
	{
		ASSERT(id.value);

		// rect2_t scroll_area = rect2_cut_right(&rect, ui_scalar(UI_SCALAR_SCROLLBAR_WIDTH));

		ui_panel_state_t *state = &ui_get_state(id)->panel;

		if (state->scrollable_height_y > 0.0f)
		{
			if (ui_is_hovered_panel(id))
			{
				state->scroll_offset_y -= ui.input.mouse_wheel;
				state->scroll_offset_y = flt_clamp(state->scroll_offset_y, 0.0f, max(0.0f, state->scrollable_height_y + ui_scalar(UI_SCALAR_WIDGET_MARGIN) - rect2_height(rect)));
			}

			// ui_id_t scrollbar_id = ui_child_id(id, S("scrollbar"));
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
			pct = ui_interpolate_f32(ui_child_id(scrollbar_id, S("jejff")), pct);

			// state->scroll_offset_y = pct*(state->scrollable_height_y - rect2_height(rect));

			float height_exclusive = scroll_area_height - handle_size;
			float handle_offset = pct*height_exclusive;

			rect2_t top    = rect2_cut_top(&scroll_area, handle_offset);
			rect2_t handle = rect2_cut_top(&scroll_area, handle_size);
			rect2_t bot    = scroll_area;

			ui_default_widget_behaviour(scrollbar_id, handle);

			v4_t color = ui_color(UI_COLOR_SLIDER_FOREGROUND);

			ui_draw_rect(top, ui_color(UI_COLOR_SLIDER_BACKGROUND));
			ui_draw_rect(handle, color);
			ui_draw_rect(bot, ui_color(UI_COLOR_SLIDER_BACKGROUND));
#endif

			offset_y = roundf(ui_interpolate_f32(ui_id_pointer(&state->scroll_offset_y), state->scroll_offset_y));
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

DREAM_INLINE v4_t ui_animate_colors(ui_id_t id, uint32_t interaction, v4_t cold, v4_t hot, v4_t active, v4_t fired)
{
	ui_id_t color_id = ui_child_id(id, S("color"));

	if (interaction & UI_FIRED)
	{
		ui_set_v4(color_id, fired);
	}

	v4_t  target    = cold;
	float stiffness = ui_scalar(UI_SCALAR_ANIMATION_STIFFNESS);

	if (interaction & UI_HOT)
	{
		target = hot;
	}

	if (interaction & UI_PRESSED)
	{
		target = active;
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

	v2_t min_widget_size;
	min_widget_size.x = ui_text_width(&ui.style.font, label);
	min_widget_size.y = ui_font_height();

	rect2_t rect = ui_cut_widget_rect(min_widget_size);
	ui_hoverable(id, rect);

	ui_interaction_t interaction = ui_default_widget_behaviour(id, rect);
	result = interaction & UI_FIRED;

	v4_t color = ui_animate_colors(id, interaction, 
								   ui_color(UI_COLOR_BUTTON_IDLE),
								   ui_color(UI_COLOR_BUTTON_HOT),
								   ui_color(UI_COLOR_BUTTON_ACTIVE),
								   ui_color(UI_COLOR_BUTTON_FIRED));

	float hover_lift = ui_button_style_hover_lift(id);

	rect2_t shadow = rect;
	rect = rect2_add_offset(rect, make_v2(0.0f, hover_lift));

	ui_draw_rect(shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_rect(rect, color);

	ui_draw_text_aligned(&ui.style.font, rect, label, make_v2(0.5f, 0.5f));

	return result;
}

bool ui_checkbox(string_t label, bool *result_value)
{
	bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	ui_id_t id = ui_id_pointer(result_value);

	float label_width   = ui_text_width(&ui.style.font, label);
	float label_padding = ui_scalar(UI_SCALAR_TEXT_MARGIN);

	v2_t min_widget_size;
	min_widget_size.y = ui_font_height();
	min_widget_size.x = label_width + label_padding + min_widget_size.y;

	rect2_t rect = ui_cut_widget_rect(min_widget_size);

	float h = rect2_height(rect);

	float checkbox_thickness        = 0.15f;
	float checkbox_thickness_pixels = checkbox_thickness*h;

	rect2_t checkbox_rect = rect2_cut_left(&rect, h);
	rect2_t check_rect = rect2_shrink(checkbox_rect, 1.5f*checkbox_thickness_pixels);
	rect2_t label_rect = rect2_cut_left(&rect, label_width + label_padding);

	rect2_t interactive_rect = rect2_union(checkbox_rect, label_rect);

	ui_hoverable(id, interactive_rect);

	ui_interaction_t interaction = ui_default_widget_behaviour(id, interactive_rect);

	if (result_value && (interaction & UI_FIRED))
	{
		*result_value = !*result_value;
	}

	bool value = (result_value ? *result_value : false);

	// --- draw checkbox ---

	// clamp roundedness to avoid the checkbox looking like a radio button and
	// transfer roundedness of outer rect to inner rect so the countours match
	float roundedness_ratio = min(0.66f, ui_roundedness_ratio(checkbox_rect));
	float outer_roundedness = ui_roundedness_ratio_to_abs(checkbox_rect, roundedness_ratio);
	float inner_roundedness = ui_roundedness_ratio_to_abs(check_rect, roundedness_ratio);

	float hover_lift = ui_button_style_hover_lift(id);

	// fudge with the color animation
	if (value && !interaction)
	{
		interaction |= UI_PRESSED;
	}

	v4_t color = ui_animate_colors(id, interaction, 
								   ui_color(UI_COLOR_BUTTON_IDLE),
								   ui_color(UI_COLOR_BUTTON_HOT),
								   ui_color(UI_COLOR_BUTTON_ACTIVE),
								   ui_color(UI_COLOR_BUTTON_FIRED));

	UI_SCALAR(UI_SCALAR_ROUNDEDNESS, outer_roundedness)
	{
		rect2_t checkbox_shadow = checkbox_rect;
		checkbox_rect = rect2_add_offset(checkbox_rect, make_v2(0, hover_lift));

		ui_draw_rect_outline(checkbox_shadow, ui_color(UI_COLOR_WIDGET_SHADOW), checkbox_thickness_pixels);
		ui_draw_rect_outline(checkbox_rect, color, checkbox_thickness_pixels);
	}

	if (value) 
	{
		UI_SCALAR(UI_SCALAR_ROUNDEDNESS, inner_roundedness)
		{
			rect2_t check_shadow = check_rect;
			check_rect = rect2_add_offset(check_rect, make_v2(0, hover_lift));

			ui_draw_rect(check_shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
			ui_draw_rect(check_rect, color);
		}
	}

	// --- draw label ---

	v2_t p = ui_text_align_p(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));
	p.x += label_padding;
	p.y += hover_lift;

	v4_t text_color = ui_color(UI_COLOR_TEXT);

	if (!value)
	{
		text_color.xyz = mul(text_color.xyz, 0.75f);
	}

	text_color = ui_interpolate_v4(ui_child_id(id, S("text_color")), text_color);

	UI_COLOR(UI_COLOR_TEXT, text_color)
	ui_draw_text(&ui.style.font, p, label);

	return result;
}

bool ui_option_buttons(string_t label, int *value, int count, string_t *options)
{
    bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	if (NEVER(!value))
		return result;

	if (NEVER(!options || count == 0))
		return result;

	ui_id_t id = ui_id(label);

	float label_width   = ui_text_width(&ui.style.font, label);
	float label_padding = 0.33f*ui_font_height();

	float button_labels_width = 0.0f;
	for (int i = 0; i < count; i++)
	{
		button_labels_width += ui_text_width(&ui.style.font, options[i]);
	}

	v2_t min_widget_size = { label_width + label_padding + button_labels_width, ui_font_height() };

	rect2_t rect = ui_cut_widget_rect(min_widget_size);

	rect2_t label_rect = rect2_cut_left(&rect, label_width + label_padding);
	label_rect = rect2_shrink(label_rect, 0.5f*ui_widget_padding());

	ui_hoverable(id, label_rect);

	ui_push_id(id);
    {
        float size = rect2_width(rect) / (float)count;
        for (int i = 0; i < count; i++)
        {
            bool selected = *value == i;

            if (selected)
            {
                ui_push_color(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_BUTTON_ACTIVE));
            }

            ui_set_next_rect(rect2_shrink(rect2_cut_left(&rect, size), ui_scalar(UI_SCALAR_WIDGET_MARGIN)));
            bool pressed = ui_button(options[i]);

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
	ui_pop_id();

	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

    return result;
}

bool ui_combo_box(string_t label, size_t *selected_index, size_t count, string_t *names)
{
	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

    bool result = false;

	if (NEVER(!ui.initialized)) 
		return result;

	if (NEVER(!selected_index))
		return result;

	if (NEVER(!names || count == 0))
		return result;

	ui_id_t id = ui_id(label);

	ui_push_id(id);

	float label_width   = ui_text_width(&ui.style.font, label);
	float label_padding = 0.33f*ui_font_height();

	float max_name_width = 0.0f;
	float total_height = 0.0f;
	float *heights = m_alloc_array_nozero(temp, count, float);
	for (size_t i = 0; i < count; i++)
	{
		v2_t dim = ui_text_dim(&ui.style.font, names[i]);
		if (max_name_width < dim.x) max_name_width = dim.x;

		float padded_height = dim.y + ui_widget_padding();
		heights[i]    = padded_height;
		total_height += padded_height;
	}

	v2_t min_widget_size = { label_width + label_padding + max_name_width, ui_font_height() };

	rect2_t rect = ui_cut_widget_rect(min_widget_size);

	rect2_t label_rect = rect2_cut_left(&rect, 0.5f*rect2_width(rect));
	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

	ui_hoverable(id, label_rect);

	rect2_t arrow_rect = rect2_cut_left(&rect, rect2_height(rect));

	bool interacted = false;

	ui_set_next_rect(arrow_rect);
	interacted |= ui_button(S(">"));

	ui_set_next_rect(rect);
	interacted |= ui_button(names[*selected_index]);

	if (interacted)
	{
		if (ui_popup_is_open(id)) 
		{
			ui_close_popup(id);
		}
		else
		{
			ui_open_popup(id);
		}
	}

	if (ui_popup_is_open(id))
	{
		const float clamped_height = min(total_height, 256.0f);

		rect2_t dropdown_rect = rect;
		dropdown_rect.max.y = rect.min.y;
		dropdown_rect.min.y = dropdown_rect.max.y - clamped_height - 2.0f*ui_scalar(UI_SCALAR_WIDGET_MARGIN);

		ui_render_layer_t old_layer = ui.render_layer;
		ui.render_layer = UI_LAYER_OVERLAY_BACKGROUND;

		ui_push_clip_rect(dropdown_rect);
		{
			rect2_t shadow_rect = rect2_add_offset(dropdown_rect, make_v2(4, -4));
			ui_draw_rect_roundedness_shadow(shadow_rect, make_v4(0, 0, 0, 0.25f), make_v4(0, 1, 0, 1), 0.25f, 16.0f);
			ui_draw_rect_roundedness_shadow(dropdown_rect, ui_color(UI_COLOR_WINDOW_BACKGROUND), make_v4(0, 1, 0, 1), 0.25f, 16.0f);
		}
		ui_pop_clip_rect();

		float margin = max(ui_scalar(UI_SCALAR_WIDGET_MARGIN), 0.5f*ui_scalar(UI_SCALAR_ROUNDEDNESS));
		dropdown_rect = rect2_shrink(dropdown_rect, margin);
		dropdown_rect = rect2_pillarbox(dropdown_rect, ui_scalar(UI_SCALAR_WINDOW_MARGIN));
		ui_push_clip_rect(dropdown_rect);

		ui_panel_begin_ex(ui_child_id(id, S("panel")), dropdown_rect, UI_PANEL_SCROLLABLE_VERT);
		{
			ui_priority_t old_priority = ui.override_priority;
			ui.override_priority = UI_PRIORITY_OVERLAY;

			ui_id_t dropdown_id = ui_id(S("dropdown"));
			ui_default_widget_behaviour(dropdown_id, dropdown_rect);

			ui_push_id(ui_id(S("options")));
			for (size_t i = 0; i < count; i++)
			{
				bool pressed = ui_button(names[i]);
				result |= pressed;
				
				if (pressed)
				{
					*selected_index = i;
				}
			}
			ui_pop_id();

			ui.render_layer = old_layer;
			ui.override_priority = old_priority;
		}
		ui_panel_end();

		ui_pop_clip_rect();
	}

	if (result)
	{
		ui.focused_id = UI_ID_NULL;
		ui_close_popup(id);
	}

	ui_pop_id();

	m_scope_end(temp);

    return result;
}

typedef enum ui_slider__type_t
{
	UI_SLIDER__F32,
	UI_SLIDER__I32,
} ui_slider__type_t;

typedef struct ui_slider__params_t
{
	ui_slider__type_t type;

	union
	{
		struct
		{
			float *v;
			float min;
			float max;
		} f32;

		struct
		{
			int32_t *v;
			int32_t min;
			int32_t max;
		} i32;
	};
} ui_slider__params_t;

DREAM_INLINE void ui_slider__impl(ui_id_t id, rect2_t rect, ui_slider__params_t *p)
{
	if (NEVER(!ui.initialized)) 
		return;

	float slider_w           = rect2_width(rect);
	float handle_w           = max(16.0f, slider_w*ui_scalar(UI_SCALAR_SLIDER_HANDLE_RATIO));
	float slider_effective_w = slider_w - handle_w;

	// TODO: This is needlessly confusing
	float relative_x = CLAMP((ui.input.mouse_p.x - ui.drag_anchor.x) - rect.min.x - 0.5f*handle_w, 0.0f, slider_effective_w);

	float t_slider = 0.0f;

	if (ui_is_active(id))
	{
		t_slider = relative_x / slider_effective_w;
		ui_set_f32(ui_child_id(id, S("t_slider")), t_slider);

		switch (p->type)
		{
			case UI_SLIDER__F32:
			{
				float value       = p->f32.min + t_slider*(p->f32.max - p->f32.min);
				float granularity = 0.01f;
				*p->f32.v = granularity*roundf(value / granularity);
			} break;

			case UI_SLIDER__I32:
			{
				*p->i32.v = p->i32.min + (int32_t)roundf(t_slider*(float)(p->i32.max - p->i32.min));
			} break;
		}
	}
	else
	{
		switch (p->type)
		{
			case UI_SLIDER__F32:
			{
				t_slider = (*p->f32.v - p->f32.min) / (p->f32.max - p->f32.min);
			} break;

			case UI_SLIDER__I32:
			{
				t_slider = (float)(*p->i32.v - p->i32.min) / (float)(p->i32.max - p->i32.min);
			} break;
		}

		t_slider = ui_interpolate_f32(ui_child_id(id, S("t_slider")), t_slider);
	}

	t_slider = saturate(t_slider);

	float handle_t = t_slider*slider_effective_w;

	rect2_t body   = rect;
	rect2_t left   = rect2_cut_left(&rect, handle_t);
	rect2_t handle = rect2_cut_left(&rect, handle_w);
	rect2_t right  = rect;

	(void)left;
	(void)right;

	ui_interaction_t interaction = ui_default_widget_behaviour(id, handle);

	v4_t color = ui_animate_colors(id, interaction, 
								   ui_color(UI_COLOR_SLIDER_FOREGROUND),
								   ui_color(UI_COLOR_SLIDER_HOT),
								   ui_color(UI_COLOR_SLIDER_ACTIVE),
								   ui_color(UI_COLOR_SLIDER_ACTIVE));

	float hover_lift = ui_hover_lift(id);

	rect2_t shadow = handle;
	handle = rect2_add_offset(handle, make_v2(0, hover_lift));

	ui_draw_rect(body, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	ui_draw_rect(shadow, ui_color(UI_COLOR_WIDGET_SHADOW));
	ui_draw_rect(handle, color);
	ui_draw_rect(right, ui_color(UI_COLOR_SLIDER_BACKGROUND));

	string_t value_text = {0};

	switch (p->type)
	{
		case UI_SLIDER__F32:
		{
			value_text = Sf("%.02f", *p->f32.v);
		} break;

		case UI_SLIDER__I32:
		{
			value_text = Sf("%d", *p->i32.v);
		} break;
	}

	ui_draw_text_aligned(&ui.style.font, body, value_text, make_v2(0.5f, 0.5f));
}

bool ui_slider(string_t label, float *v, float min, float max)
{
	if (NEVER(!ui.initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	float init_v = *v;

	ui_id_t id = ui_id_pointer(v);

	v2_t min_widget_size;
	min_widget_size.x = ui_text_width(&ui.style.font, label) + 32.0f;
	min_widget_size.y = ui_font_height();

	rect2_t rect = ui_cut_widget_rect(min_widget_size);
	ui_hoverable(id, rect);

	rect2_t label_rect = rect2_cut_left(&rect, 0.5f*rect2_width(rect));

	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

	ui_slider__params_t p = {
		.type = UI_SLIDER__F32,
		.f32 = {
			.v   = v,
			.min = min,
			.max = max,
		},
	};

	ui_slider__impl(id, rect, &p);

	return *v != init_v;
}

bool ui_slider_int_ex(string_t label, int *v, int min, int max, ui_slider_flags_t flags)
{
	if (NEVER(!ui.initialized)) 
		return false;

	if (NEVER(!v)) 
		return false;

	int init_value = *v;

	ui_id_t id = ui_id_pointer(v);

	float widget_margin = ui_scalar(UI_SCALAR_WIDGET_MARGIN);

	v2_t min_widget_size;
	min_widget_size.y = ui_font_height();
	min_widget_size.x = ui_text_width(&ui.style.font, label) + 32.0f + 2.0f*min_widget_size.y + 2.0f*widget_margin;

	rect2_t rect = ui_cut_widget_rect(min_widget_size);
	ui_hoverable(id, rect);

	rect2_t label_rect = rect2_cut_left(&rect, 0.5f*rect2_width(rect));

	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

	if (flags & UI_SLIDER_FLAGS_INC_DEC_BUTTONS)
	{
		rect2_t sub_rect = rect2_cut_left(&rect, rect2_height(rect));
		rect2_cut_left(&rect, widget_margin);

		rect2_t add_rect = rect2_cut_right(&rect, rect2_height(rect));
		rect2_cut_right(&rect, widget_margin);

		ui_push_id(id);
		{
			ui_set_next_rect(sub_rect);
			if (ui_button(S("-")))
			{
				if (*v > min) *v = *v - 1;
			}

			ui_set_next_rect(add_rect);
			if (ui_button(S("+")))
			{
				if (*v < max) *v = *v + 1;
			}
		}
		ui_pop_id();
	}

	ui_slider__params_t p = {
		.type = UI_SLIDER__I32,
		.i32 = {
			.v   = v,
			.min = min,
			.max = max,
		},
	};

	ui_slider__impl(id, rect, &p);

	return *v != init_value;
}

bool ui_slider_int(string_t label, int *v, int min, int max)
{
	return ui_slider_int_ex(label, v, min, max, UI_SLIDER_FLAGS_INC_DEC_BUTTONS);
}

DREAM_INLINE size_t ui_text_edit__find_insert_point_from_offset(prepared_glyphs_t *prep, float x)
{
	size_t result = 0;

	for (size_t i = 0; i < prep->count; i++)
	{
		prepared_glyph_t *glyph = &prep->glyphs[i];

		float p = 0.5f*(glyph->rect.min.x + glyph->rect.max.x);
		if (x >= p)
		{
			result = i + 1;
		}
		else
		{
			break;
		}
	}

	return result;
}

DREAM_INLINE float ui_text_edit__get_caret_x(prepared_glyphs_t *prep, size_t index)
{
	float result = 0.0f;

	if (prep->count > 0)
	{
		if (index < prep->count)
		{
			result = prep->glyphs[index].rect.min.x;
		}
		else
		{
			result = prep->glyphs[prep->count - 1].rect.max.x;
		}
	}

	return result;
}

void ui_text_edit(string_t label, dynamic_string_t *buffer)
{
	if (NEVER(!ui.initialized)) 
		return;

	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

	// TODO: Use ui_cut_widget_rect 
	rect2_t rect;
	if (!ui_override_rect(&rect))
	{
		ui_panel_t *panel = ui_panel();
		rect = rect2_cut_top(&panel->rect_layout, ui_font_height() + ui_widget_padding());
	}

	ui_id_t id = ui_id_pointer(buffer);
	ui_push_id(id);

	ui_hoverable(id, rect);

	rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

	rect2_t label_rect = rect2_cut_left(&rect, 0.5f*rect2_width(rect));
	label_rect = rect2_shrink(label_rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	ui_text_edit_state_t *state = &ui_get_state(id)->text_edit;

	ui_interaction_t interaction = ui_default_widget_behaviour(id, rect);

	if (ui_is_hot(id))
	{
		ui.input.cursor = PLATFORM_CURSOR_TEXT_INPUT;
	}

	bool inserted_character = false;
	bool has_focus          = ui_id_has_focus(id);
	bool selection_active   = state->cursor != state->selection_start;

	if (has_focus)
	{
		for (platform_event_t *event = platform_event_iter(ui.input.events);
			 event;
			 event = platform_event_next(event))
		{
			switch (event->kind)
			{
				case PLATFORM_EVENT_TEXT:
				{
					for (size_t i = 0; i < event->text.text.count; i++)
					{
						// TODO: Unicode
						char c = event->text.text.data[i];

						if (c >= 32 && c < 127)
						{
							bool success = dyn_string_insertc(buffer, (int)state->cursor, c);

							if (success)
							{
								state->cursor += 1;
								state->selection_start = state->cursor;

								inserted_character = true;
							}
						}
					}

					platform_consume_event(event);
				} break;

				case PLATFORM_EVENT_KEY:
				{
					if (event->key.pressed)
					{
						switch (event->key.keycode)
						{
							case PLATFORM_KEYCODE_LEFT:
							{
								if (selection_active && !event->shift)
								{
									size_t start = state->selection_start;
									size_t end   = state->cursor;

									if (start > end) SWAP(size_t, start, end);

									state->cursor = state->selection_start = start;
								}
								else
								{
									if (state->cursor > 0)
									{
										state->cursor -= 1;
									}

									if (!event->shift) 
									{
										state->selection_start = state->cursor;
									}
								}

								platform_consume_event(event);
							} break;

							case PLATFORM_KEYCODE_RIGHT:
							{
								if (selection_active && !event->shift)
								{
									size_t start = state->selection_start;
									size_t end   = state->cursor;

									if (start > end) SWAP(size_t, start, end);

									state->cursor = state->selection_start = end;
								}
								else
								{
									state->cursor += 1;

									if (state->cursor > buffer->count) state->cursor = (int)buffer->count;

									if (!event->shift)
									{
										state->selection_start = state->cursor;
									}

									platform_consume_event(event);
								}
							} break;

							case PLATFORM_KEYCODE_DELETE:
							case PLATFORM_KEYCODE_BACKSPACE:
							{
								bool delete    = event->key.keycode == PLATFORM_KEYCODE_DELETE;
								bool backspace = event->key.keycode == PLATFORM_KEYCODE_BACKSPACE;

								size_t start = (size_t)state->selection_start;
								size_t end   = (size_t)state->cursor;

								if (start == end)
								{
									if (backspace && start > 0) start -= 1;
									if (delete                ) end   += 1;
								}

								if (start > end) SWAP(size_t, start, end);

								size_t remove_count = end - start;

								dyn_string_remove_range(buffer, start, remove_count);

								state->cursor          = start;
								state->selection_start = state->cursor;

								if (delete)    debug_notif(COLORF_BLUE, 2.0f, Sf("delete    (s: %zu, e: %zu (#%zu))", start, end, remove_count));
								if (backspace) debug_notif(COLORF_BLUE, 2.0f, Sf("backspace (s: %zu, e: %zu (#%zu))", start, end, remove_count));
							} break;

							case 'A':
							{
								if (event->ctrl)
								{
									state->selection_start = 0;
									state->cursor          = buffer->count;
									platform_consume_event(event);
								}
							} break;
						}
					}
				} break;
			}
		}
	}

	string_t string      = buffer->string;
	rect2_t  buffer_rect = rect2_shrink(rect, ui_scalar(UI_SCALAR_TEXT_MARGIN));

	ui_draw_rect(rect, ui_color(UI_COLOR_SLIDER_BACKGROUND));
	ui_draw_text_aligned(&ui.style.font, buffer_rect, string, make_v2(0.0f, 0.5f));
	ui_draw_text_aligned(&ui.style.font, label_rect, label, make_v2(0.0f, 0.5f));

	if (has_focus)
	{
		prepared_glyphs_t prep = atlas_prepare_glyphs(&ui.style.font, temp, string);

		if (prep.count > 0)
		{
			if (interaction & UI_PRESSED)
			{
				float mouse_offset_x = ui.input.mouse_p.x - buffer_rect.min.x;
				state->cursor = (int)ui_text_edit__find_insert_point_from_offset(&prep, mouse_offset_x);
				state->selection_start = state->cursor;
			}
			else if (interaction & UI_HELD)
			{
				float mouse_offset_x = ui.input.mouse_p.x - buffer_rect.min.x;
				state->cursor = (int)ui_text_edit__find_insert_point_from_offset(&prep, mouse_offset_x);
			}

			size_t selection_min  = MIN(state->selection_start, state->cursor);
			size_t selection_max  = MAX(state->selection_start, state->cursor);
			size_t selection_size = selection_max - selection_min;

			float cursor_p = buffer_rect.min.x + ui_text_edit__get_caret_x(&prep, state->cursor);

			float selection_start_p = buffer_rect.min.x + ui_text_edit__get_caret_x(&prep, state->selection_start);
			float selection_end_p   = cursor_p;

			if (selection_start_p > selection_end_p)
			{
				SWAP(float, selection_start_p, selection_end_p);
			}

			rect2_t cursor_rect = rect2_from_min_dim(make_v2(cursor_p - 1.0f, buffer_rect.min.y + 2.0f), make_v2(2.0f, rect2_height(buffer_rect) - 4.0f));
			rect2_t selection_rect = rect2_min_max(make_v2(selection_start_p, buffer_rect.min.y), make_v2(selection_end_p, buffer_rect.max.y));

			v4_t caret_color = ui_color(UI_COLOR_TEXT);
			caret_color.w = 0.75f;

			if (selection_size == 0)
			{
				ui_draw_rect_roundedness(cursor_rect, caret_color, v4_from_scalar(0.0f));
			}

			ui_draw_rect(selection_rect, make_v4(0.5f, 0.0f, 0.0f, 0.5));
		}
		else
		{
			v4_t caret_color = ui_color(UI_COLOR_TEXT);
			caret_color.w = 0.75f;

			ui_draw_rect_roundedness(rect2_from_min_dim(make_v2(buffer_rect.min.x - 1.0f, buffer_rect.min.y + 2.0f),
														make_v2(2.0f, rect2_height(buffer_rect) - 4.0f)), 
									 caret_color, v4_from_scalar(0.0f));
		}
	}

	ui_pop_id();

	m_scope_end(temp);
}

void ui_tooltip(string_t text)
{
	if (!stack_full(ui.tooltip_stack))
	{
		ui_tooltip_t *tooltip = stack_add(ui.tooltip_stack);
		string_into_storage(tooltip->text, text);
	}
}

void ui_hover_tooltip(string_t text)
{
	string_into_storage(ui.next_tooltip, text);
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

bool ui_is_hovered_panel(ui_id_t id)
{
	return ui.hovered_panel.value == id.value;
}

void ui_set_hot(ui_id_t id)
{
	if (!ui.active.value)
	{
		ui.hot = id;
	}
}

void ui_set_next_hot(ui_id_t id, ui_priority_t priority)
{
	if (ui.next_hot_priority <= priority)
	{
		ui.next_hot          = id;
		ui.next_hot_priority = priority;
	}
}

void ui_set_active(ui_id_t id)
{
	ui.active = id;
	ui.focused_id = id;
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

bool ui_has_focus(void)
{
	return ui.has_focus && ui.input.app_has_focus;
}

bool ui_id_has_focus(ui_id_t id)
{
	return ui.has_focus && ui.focused_id.value == id.value;
}

ui_state_t *ui_get_state(ui_id_t id)
{
	ui_state_t *result = table_find_object(&ui.state_index, id.value);

	if (!result)
	{
		result = pool_add(&ui.state);
		result->id                  = id;
		result->created_frame_index = ui.frame_index;
		table_insert_object(&ui.state_index, id.value, result);
	}

	ASSERT(result->id.value == id.value);

	result->last_touched_frame_index = ui.frame_index;
	return result;
}

bool ui_state_is_new(ui_state_t *state)
{
	return state->created_frame_index == ui.frame_index;
}

void ui_hoverable(ui_id_t id, rect2_t rect)
{
	if (ui_mouse_in_rect(rect))
	{
		ui.next_hovered_widget = id;
	}

	string_t next_tooltip = string_from_storage(ui.next_tooltip);

	if (!string_empty(next_tooltip) && 
		ui_is_hovered_delay(id, ui_scalar(UI_SCALAR_TOOLTIP_DELAY)))
	{
		ui_tooltip(next_tooltip);
	}

	ui.next_tooltip.count = 0;
}

bool ui_is_hovered(ui_id_t id)
{
	return ui.hovered_widget.value == id.value;
}

bool ui_is_hovered_delay(ui_id_t id, float delay)
{
	return ui_is_hovered(id) && (ui.hover_time_seconds >= delay);
}

static void ui_initialize(void)
{
	ASSERT(!ui.initialized);

	ui.style.font_data        = fs_read_entire_file(&ui.arena, S("gamedata/fonts/NotoSans/NotoSans-Regular.ttf"));
	ui.style.header_font_data = fs_read_entire_file(&ui.arena, S("gamedata/fonts/NotoSans/NotoSans-Bold.ttf"));
	ui_set_font_height(18.0f);

	v4_t background    = make_v4(0.20f, 0.15f, 0.17f, 1.0f);
	v4_t background_hi = make_v4(0.22f, 0.18f, 0.18f, 1.0f);
	v4_t foreground    = make_v4(0.33f, 0.28f, 0.28f, 1.0f);

	v4_t hot    = make_v4(0.25f, 0.45f, 0.40f, 1.0f);
	v4_t active = make_v4(0.30f, 0.55f, 0.50f, 1.0f);
	v4_t fired  = make_v4(0.45f, 0.65f, 0.55f, 1.0f);

	ui.style.base_scalars[UI_SCALAR_TOOLTIP_DELAY         ] = 0.0f;
	ui.style.base_scalars[UI_SCALAR_ANIMATION_RATE        ] = 40.0f;
	ui.style.base_scalars[UI_SCALAR_ANIMATION_STIFFNESS   ] = 512.0f;
	ui.style.base_scalars[UI_SCALAR_ANIMATION_DAMPEN      ] = 32.0f;
	ui.style.base_scalars[UI_SCALAR_ANIMATION_LENGTH_LIMIT] = FLT_MAX;
	ui.style.base_scalars[UI_SCALAR_HOVER_LIFT            ] = 1.5f;
	ui.style.base_scalars[UI_SCALAR_WINDOW_MARGIN         ] = 0.0f;
	ui.style.base_scalars[UI_SCALAR_WIDGET_MARGIN         ] = 1.0f;
	ui.style.base_scalars[UI_SCALAR_TEXT_MARGIN           ] = 2.2f;
	ui.style.base_scalars[UI_SCALAR_ROUNDEDNESS           ] = 2.5f;
	ui.style.base_scalars[UI_SCALAR_TEXT_ALIGN_X          ] = 0.5f;
	ui.style.base_scalars[UI_SCALAR_TEXT_ALIGN_Y          ] = 0.5f;
	ui.style.base_scalars[UI_SCALAR_SCROLLBAR_WIDTH       ] = 12.0f;
	ui.style.base_scalars[UI_SCALAR_SLIDER_HANDLE_RATIO   ] = 1.0f / 4.0f;
	ui.style.base_colors [UI_COLOR_TEXT                   ] = make_v4(0.95f, 0.90f, 0.85f, 1.0f);
	ui.style.base_colors [UI_COLOR_TEXT_SHADOW            ] = make_v4(0.00f, 0.00f, 0.00f, 0.50f);
	ui.style.base_colors [UI_COLOR_WIDGET_SHADOW          ] = make_v4(0.00f, 0.00f, 0.00f, 0.20f);
	ui.style.base_colors [UI_COLOR_WINDOW_BACKGROUND      ] = background;
	ui.style.base_colors [UI_COLOR_WINDOW_TITLE_BAR       ] = make_v4(0.45f, 0.25f, 0.25f, 1.0f);
	ui.style.base_colors [UI_COLOR_WINDOW_TITLE_BAR_HOT   ] = make_v4(0.45f, 0.22f, 0.22f, 1.0f);
	ui.style.base_colors [UI_COLOR_WINDOW_CLOSE_BUTTON    ] = make_v4(0.35f, 0.15f, 0.15f, 1.0f);
	ui.style.base_colors [UI_COLOR_WINDOW_OUTLINE         ] = make_v4(0.1f, 0.1f, 0.1f, 0.20f);
	ui.style.base_colors [UI_COLOR_PROGRESS_BAR_EMPTY     ] = background_hi;
	ui.style.base_colors [UI_COLOR_PROGRESS_BAR_FILLED    ] = hot;
	ui.style.base_colors [UI_COLOR_BUTTON_IDLE            ] = foreground;
	ui.style.base_colors [UI_COLOR_BUTTON_HOT             ] = hot;
	ui.style.base_colors [UI_COLOR_BUTTON_ACTIVE          ] = active;
	ui.style.base_colors [UI_COLOR_BUTTON_FIRED           ] = fired;
	ui.style.base_colors [UI_COLOR_SLIDER_BACKGROUND      ] = background_hi;
	ui.style.base_colors [UI_COLOR_SLIDER_FOREGROUND      ] = foreground;
	ui.style.base_colors [UI_COLOR_SLIDER_HOT             ] = hot;
	ui.style.base_colors [UI_COLOR_SLIDER_ACTIVE          ] = active;

	size_t text_input_capacity = 64;

	ui.input.text = (dynamic_string_t){
		.capacity = text_input_capacity,
		.count    = 0,
		.data     = m_alloc_nozero(&ui.arena, text_input_capacity, 16),
	};

	ui.render_commands.capacity = UI_RENDER_COMMANDS_CAPACITY;
	ui.render_commands.keys     = m_alloc_array_nozero(&ui.arena, ui.render_commands.capacity, ui_render_command_key_t);
	ui.render_commands.commands = m_alloc_array_nozero(&ui.arena, ui.render_commands.capacity, ui_render_command_t);

	ui.initialized = true;
}

bool ui_begin(float dt)
{
	if (!ui.initialized)
	{
		ui_initialize();
	}

	ui.input.dt = dt;

	ui.last_frame_ui_rect_count = ui.render_commands.count;
    ui_reset_render_commands();

	for (pool_iter_t it = pool_iter(&ui.state); 
		 pool_iter_valid(&it); 
		 pool_iter_next(&it))
	{
		ui_state_t *state = it.data;

		if (state->last_touched_frame_index < ui.frame_index)
		{
			table_remove(&ui.state_index, state->id.value);
			pool_rem_item(&ui.state, state);
		}
	}

	for (pool_iter_t it = pool_iter(&ui.style.animation_state); 
		 pool_iter_valid(&it); 
		 pool_iter_next(&it))
	{
		ui_anim_t *anim = it.data;

		if (anim->last_touched_frame_index >= ui.frame_index)
		{
			float length_limit = anim->length_limit;
			float c_t = anim->c_t;
			float c_v = anim->c_v;

			v4_t target   = anim->t_target;
			v4_t current  = anim->t_current;
			v4_t velocity = anim->t_velocity;

			if (length_limit != FLT_MAX)
			{
				v4_t min = sub(target, length_limit);
				v4_t max = add(target, length_limit);
				current = v4_min(current, max);
				current = v4_max(current, min);
			}

			v4_t accel_t = mul( c_t, sub(target, current));
			v4_t accel_v = mul(-c_v, velocity);
			v4_t accel = add(accel_t, accel_v);

			velocity = add(velocity, mul(ui.input.dt, accel));
			current  = add(current,  mul(ui.input.dt, velocity));

			anim->t_current  = current;
			anim->t_velocity = velocity;
		}
		else
		{
			table_remove (&ui.style.animation_index, anim->id.value);
			pool_rem_item(&ui.style.animation_state, anim);
		}
	}


	ui.frame_index += 1;
	
	// now that the frame index is incremented, clear the current frame arena
	arena_t *frame_arena = ui_frame_arena();
	m_reset_and_decommit(frame_arena);

	ui.current_time = os_hires_time();

    if (!ui.active.value)
	{
		if (ui.next_hovered_widget.value &&
			ui.next_hovered_widget.value != ui.hovered_widget.value)
		{
			ui.hover_time = ui.current_time;
		}

		ui.hot            = ui.next_hot;
		ui.hovered_panel  = ui.next_hovered_panel;
		ui.hovered_widget = ui.next_hovered_widget;
	}

	ui.hover_time_seconds = 0.0f;

	if (ui.hovered_widget.value)
	{
		ui.hover_time_seconds = (float)os_seconds_elapsed(ui.hover_time, ui.current_time);
	}

	ui.next_hot            = UI_ID_NULL;
	ui.next_hot_priority   = UI_PRIORITY_DEFAULT;
	ui.next_hovered_panel  = UI_ID_NULL;
	ui.next_hovered_widget = UI_ID_NULL;

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
    int res_x, res_y;
    render->get_resolution(&res_x, &res_y);

	float font_height = ui_font_height();
	float at_y = 32.0f;

	for (size_t i = 0; i < stack_count(ui.tooltip_stack); i++)
	{
		ui_tooltip_t *tooltip = &ui.tooltip_stack.values[i];

		string_t text = string_from_storage(tooltip->text);

		float text_w = ui_text_width(&ui.style.font, text);

		float center_x = 0.5f*(float)res_x;
		rect2_t rect = rect2_center_dim(make_v2(center_x, at_y), make_v2(4.0f + text_w, font_height));

		ui_draw_rect_shadow(rect, make_v4(0, 0, 0, 0.5f), 0.20f, 32.0f);
		ui_draw_text_aligned(&ui.style.font, rect, text, make_v2(0.5f, 0.5f));
		at_y += ui.style.font.y_advance;
	}

	stack_reset(ui.tooltip_stack);

	debug_notif_t *notifs           = ui.first_debug_notif;
	debug_notif_t *surviving_notifs = NULL;

	v2_t notif_at = { 4, res_y - 4 };

	ui.first_debug_notif = NULL;
	while (notifs)
	{
		debug_notif_t *notif = sll_pop(notifs);

		v2_t text_dim = ui_text_dim(&ui.style.font, notif->text);

		v2_t pos = add(notif_at, make_v2(0, -text_dim.y));

		float lifetime_t = notif->lifetime / notif->init_lifetime;

		v4_t color = notif->color;
		color.w *= smoothstep(saturate(4.0f * lifetime_t));

		UI_COLOR(UI_COLOR_TEXT, color)
		ui_draw_text(&ui.style.font, pos, notif->text);

		notif_at.y -= text_dim.y + 4.0;

		notif->lifetime -= ui.input.dt;
		if (notif->lifetime > 0.0f)
		{
			sll_push(surviving_notifs, notif);
		}
	}

	while (surviving_notifs)
	{
		debug_notif_t *notif = sll_pop(surviving_notifs);
		debug_notif_replicate(notif);
	}

	for (platform_event_t *event = platform_event_iter(ui.input.events);
		 event;
		 event = platform_event_next(event))
	{
		switch (event->kind)
		{
			case PLATFORM_EVENT_MOUSE_BUTTON:
			{
				if (event->mouse_button.pressed)
				{
					ui.has_focus = ui.hovered;
					platform_consume_event(event);
				}
			} break;

			case PLATFORM_EVENT_KEY:
			{
				if (event->key.pressed)
				{
					if (event->key.keycode == PLATFORM_KEYCODE_ESCAPE)
					{
						if (ui_id_valid(ui.focused_id))
						{
							ui.focused_id = UI_ID_NULL;
							platform_consume_event(event);
						}
						else if (ui_has_focus())
						{
							ui.has_focus = false;
							platform_consume_event(event);
						}
					}
				}
			} break;
		}
	}

	ui.input.mouse_wheel            = 0.0f;
	ui.input.mouse_buttons_pressed  = 0;
	ui.input.mouse_buttons_released = 0;
	dyn_string_clear(&ui.input.text);

	ASSERT(ui.panels.current_panel == NULL);
	ASSERT(ui.id_stack.at == 0);

	ui_sort_render_commands();
}

ui_render_command_list_t *ui_get_render_commands(void)
{
    return &ui.render_commands;
}

void debug_text(v4_t color, string_t fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	debug_text_va(color, fmt, args);
	va_end(args);
}

void debug_text_va(v4_t color, string_t fmt, va_list args)
{
	debug_notif_va(color, 0.0f, fmt, args);
}

void debug_notif(v4_t color, float time, string_t fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	debug_notif_va(color, time, fmt, args);
	va_end(args);
}

void debug_notif_va(v4_t color, float time, string_t fmt, va_list args)
{
	m_scoped_temp
	{
		string_t text = string_format(ui_frame_arena(), string_null_terminate(temp, fmt), args);

		debug_notif_t *notif = m_alloc_struct(ui_frame_arena(), debug_notif_t);
		notif->id            = ui_id_u64(ui.next_notif_id++);
		notif->color         = color;
		notif->text          = text;
		notif->init_lifetime = time;
		notif->lifetime      = time;

		notif->next = ui.first_debug_notif;
		ui.first_debug_notif = notif;
	}
}

void debug_notif_replicate(debug_notif_t *notif)
{
	debug_notif_t *repl = m_copy_struct(ui_frame_arena(), notif);
	repl->text = string_copy(ui_frame_arena(), repl->text);

	repl->next = ui.first_debug_notif;
	ui.first_debug_notif = repl;
}
