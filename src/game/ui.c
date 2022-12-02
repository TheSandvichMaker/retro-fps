#include "ui.h"
#include "input.h"
#include "core/arena.h"
#include "core/string.h"
#include "render/render.h"
#include "render/render_helpers.h"

typedef struct ui_state_t
{
    arena_t frame_arena;
    arena_t arena;

    uint64_t frame_index;
    uint64_t next_box_debug_ordinal;

    ui_box_t *first_free_box;
    ui_box_t *box_hash[4096];

    ui_box_t *root;
    ui_box_t *current_parent;

    v2_t element_margins;
    v2_t    text_margins;

    ui_style_t style;

    bitmap_font_t *font;

    ui_box_t *hot;
    ui_box_t *active;

    v2_t mouse_p;
    v2_t mouse_dp;
    v2_t press_p;
    v2_t screen_dim;

    ui_style_node_t *first_free_style;
    ui_style_node_t *first_style;

    bool has_focus;
} ui_state_t;

static ui_state_t g_ui;

static inline string_t ui_display_text(string_t key_string)
{
    string_t result = substring(key_string, 0, string_find_first(key_string, strlit("##")));
    return result;
}

static inline ui_key_t ui_key(string_t string)
{
    ui_key_t seed = (g_ui.current_parent ? g_ui.current_parent->key : 0);
    return string_hash_with_seed(string, seed);
}

static ui_box_t *ui_get_or_create_box(string_t key_string)
{
    bool anonymous = key_string.count == 0;

    ui_key_t key   = ui_key(key_string);
    uint64_t index = key % ARRAY_COUNT(g_ui.box_hash); 

    ui_box_t *box = NULL;

    if (!anonymous)
    {
        for (ui_box_t *hash_probe = g_ui.box_hash[index];
             hash_probe;
             hash_probe = hash_probe->next_in_hash)
        {
            if (hash_probe->key == key)
            {
                box = hash_probe;
                break;
            }
        }
    }

    if (!box)
    {
        if (anonymous)
        {
            box = m_alloc_struct(&g_ui.frame_arena, ui_box_t);
            box->key_string = strlit("anonymous");
        }
        else
        {
            if (!g_ui.first_free_box)
            {
                g_ui.first_free_box = m_alloc_struct(&g_ui.arena, ui_box_t);
            }
            box = sll_pop(g_ui.first_free_box);
            zero_struct(box);

            box->next_in_hash = g_ui.box_hash[index];
            g_ui.box_hash[index] = box;
        }
    }

    box->key        = key;
    box->key_string = string_copy(&g_ui.frame_arena, key_string);

    box->debug_ordinal = g_ui.next_box_debug_ordinal++;

    box->parent = g_ui.current_parent;
    ASSERT(box->parent != box);

    box->first = NULL;
    box->last  = NULL;
    box->next  = NULL;
    box->prev  = NULL;

    box->computed_rel_position[AXIS2_X] = 0.0f;
    box->computed_rel_position[AXIS2_Y] = 0.0f;
    box->computed_size[AXIS2_X] = 0.0f;
    box->computed_size[AXIS2_Y] = 0.0f;

    box->style = ui_get_style();

    if (box->parent)
        dll_push_back(box->parent->first, box->parent->last, box);

    ASSERT(box->first != box);

    box->last_touched_frame = g_ui.frame_index;

    return box;
}

bool ui_begin(bitmap_font_t *font, const ui_style_t *style)
{
    m_reset(&g_ui.frame_arena);

    int resolution_x, resolution_y;
    render->get_resolution(&resolution_x, &resolution_y);

    g_ui.screen_dim = make_v2((float)resolution_x, (float)resolution_y);

    for (size_t slot = 0; slot < ARRAY_COUNT(g_ui.box_hash); slot++)
    {
        for (ui_box_t **box_at = &g_ui.box_hash[slot]; *box_at;)
        {
            ui_box_t *box = *box_at;

            if (box->last_touched_frame < g_ui.frame_index)
            {
                *box_at = box->next_in_hash;
                sll_push(g_ui.first_free_box, box);
            }
            else
            {
                box_at = &(*box_at)->next_in_hash;
            }
        }
    }

    g_ui.element_margins = make_v2(2.0f, 2.0f);
    g_ui.text_margins = make_v2(2.0f, 2.0f);

    g_ui.first_style = NULL;
    ui_push_style(style);

    g_ui.current_parent = NULL;

    g_ui.root = ui_get_or_create_box(strlit("root"));
    g_ui.current_parent = g_ui.root;

    g_ui.root->layout_axis = AXIS2_Y;
    g_ui.root->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PIXELS, g_ui.screen_dim.x, 1.0f };
    g_ui.root->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_PIXELS, g_ui.screen_dim.y, 1.0f };

    int mouse_x, mouse_y;
    get_mouse(&mouse_x, &mouse_y);

    g_ui.mouse_p = make_v2((float)mouse_x, (float)mouse_y);

    if (ui_button_pressed(BUTTON_FIRE1))
        g_ui.press_p = g_ui.mouse_p;

    int mouse_dx, mouse_dy;
    get_mouse_delta(&mouse_dx, &mouse_dy);

    g_ui.mouse_dp = make_v2((float)mouse_dx, (float)mouse_dy);
    g_ui.mouse_dp.y = -g_ui.mouse_dp.y;

    g_ui.font = font;

    g_ui.frame_index += 1;

    if (ui_button_pressed(BUTTON_FIRE1|BUTTON_FIRE2))
    {
        if (g_ui.hot)
        {
            g_ui.active = g_ui.hot;
            g_ui.has_focus = g_ui.hot != g_ui.root;
        }
        else
        {
            g_ui.has_focus = false;
        }
    }

    return g_ui.has_focus;
}

ui_interaction_t ui_interaction_from_box(ui_box_t *box)
{
    ui_interaction_t result = { 0 };

    if (box == g_ui.hot)
    {
        result.mouse_p       = g_ui.mouse_p;
        result.press_p       = g_ui.press_p;
        result.drag_delta    = g_ui.mouse_dp;
        result.clicked       = ui_button_pressed(BUTTON_FIRE1);
        result.right_clicked = ui_button_pressed(BUTTON_FIRE2);
        result.hovering      = true;
    }

    if (box == g_ui.active)
    {
        result.drag_delta = g_ui.mouse_dp;
        result.dragging = !!(box->flags & UI_DRAGGABLE);
        result.released = ui_button_released(BUTTON_FIRE1);
    }

    return result;
}

axis2_t ui_layout_axis(void)
{
    return g_ui.current_parent->layout_axis;
}

axis2_t ui_cross_axis(void)
{
    return (g_ui.current_parent->layout_axis + 1) % 2;
}

ui_box_t *ui_push_parent(ui_box_t *parent)
{
    ui_box_t *result = g_ui.current_parent;

    g_ui.current_parent = parent;

    return result;
}

ui_box_t *ui_pop_parent(void)
{
    ui_box_t *result = g_ui.current_parent;

    if (ALWAYS(result))
        g_ui.current_parent = result->parent;

    return result;
}

static inline float ui_font_height(void)
{
    return (float)g_ui.font->ch;
}

static inline float ui_standard_element_height(void)
{
    return (float)g_ui.font->ch + 2.0f*g_ui.element_margins.y + 2.0f*g_ui.text_margins.y;
}

ui_style_t ui_get_style(void)
{
    ui_style_t result = { 0 };

    if (ALWAYS(g_ui.first_style))
    {
        copy_struct(&result, &g_ui.first_style->style);
    }

    return result;
}

void ui_push_style(const ui_style_t *style)
{
    if (!g_ui.first_free_style)
    {
        g_ui.first_free_style = m_alloc_struct_nozero(&g_ui.frame_arena, ui_style_node_t);
        g_ui.first_free_style->next = NULL;
    }

    ui_style_node_t *node = sll_pop(g_ui.first_free_style);
    copy_struct(&node->style, style); 

    sll_push(g_ui.first_style, node);
}

void ui_pop_style(void)
{
    if (ALWAYS(g_ui.first_style))
    {
        sll_pop(g_ui.first_style);
    }
}

void ui_push_background_color(ui_gradient_t color)
{
    ui_style_t style = ui_get_style();
    style.background_color = color;

    ui_push_style(&style);
}

ui_box_t *ui_box_margins(float size)
{
    axis2_t layout_axis = ui_layout_axis();

    ui_box_t *outer = ui_box(strnull, 0);
    ui_set_size(outer, AXIS2_X, ui_pct(1.0f, 1.0f));
    ui_set_size(outer, AXIS2_Y, ui_pct(1.0f, 1.0f));
    outer->layout_axis = AXIS2_Y;
    outer->key_string = strlit("BOX MARGINS OUTER");

    ui_box_t *content_area = NULL;

    UI_Parent(outer)
    {
        ui_box_t *top = ui_box(strnull, 0);
        ui_set_size(top, AXIS2_X, ui_pct(1.0f, 1.0f));
        ui_set_size(top, AXIS2_Y, ui_pixels(size, 1.0f));

        ui_box_t *middle = ui_box(strnull, 0);
        ui_set_size(middle, AXIS2_X, ui_pct(1.0f, 1.0f));
        ui_set_size(middle, AXIS2_Y, ui_pct(1.0f, 0.0f));
        middle->layout_axis = AXIS2_X;

        UI_Parent(middle)
        {
            ui_box_t *left = ui_box(strnull, 0);
            ui_set_size(left, AXIS2_X, ui_pixels(size, 1.0f));
            ui_set_size(left, AXIS2_Y, ui_pct(1.0f, 1.0f));

            content_area = ui_box(strnull, 0);
            ui_set_size(content_area, AXIS2_X, ui_pct(1.0f, 0.0f));
            ui_set_size(content_area, AXIS2_Y, ui_pct(1.0f, 1.0f));
            content_area->layout_axis = layout_axis;

            ui_box_t *right = ui_box(strnull, 0);
            ui_set_size(right, AXIS2_X, ui_pixels(size, 1.0f));
            ui_set_size(right, AXIS2_Y, ui_pct(1.0f, 1.0f));
        }

        ui_box_t *bottom = ui_box(strnull, 0);
        ui_set_size(bottom, AXIS2_Y, ui_pixels(size, 1.0f));
        ui_set_size(bottom, AXIS2_X, ui_pct(1.0f, 1.0f));
    }

    return content_area;
}

ui_box_t *ui_box(string_t key, uint32_t flags)
{
    ui_box_t *box = ui_get_or_create_box(key);

    box->flags       = flags;
    box->layout_axis = g_ui.current_parent->layout_axis;
    box->text        = string_copy(&g_ui.frame_arena, ui_display_text(key));

    ui_set_size(box, ui_cross_axis(), ui_pct(1.0f, 0.0f));

    return box;
}

ui_box_t *ui_window_begin(string_t text, uint32_t flags, float x, float y, float w, float h)
{
    ui_box_t *box = ui_box(string_format(temp, "%.*s##panel", strexpand(text)), UI_DRAW_BACKGROUND|UI_DRAW_OUTLINE|flags);

    box->position_offset[AXIS2_X] = x;
    box->position_offset[AXIS2_Y] = y;

    box->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PIXELS, w, 1.0f };
    box->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_PIXELS, h, 1.0f };
    box->rect.min = make_v2(x, y);

    ui_push_parent(box);

    ui_box_t *title_bar = ui_box(text, UI_DRAW_BACKGROUND|UI_DRAW_TEXT);
    title_bar->style.background_color = ui_gradient_vertical(make_v4(0.35f, 0.15f, 0.15f, 1.0f), make_v4(0.25f, 0.10f, 0.10f, 1.0f));
    ui_set_size(title_bar, AXIS2_Y, ui_pixels(16.0f, 1.0f));

    // ui_push_parent(ui_box_margins(4.0f));

    return box;
}

void ui_window_end(void)
{
    ui_pop_parent();
    ui_pop_parent();
}

ui_box_t *ui_label(string_t text, uint32_t flags)
{
    ui_box_t *box = ui_box(text, UI_DRAW_TEXT|flags);

    ui_set_size(box, AXIS2_X, ui_txt(1.0f));
    ui_set_size(box, AXIS2_Y, ui_txt(1.0f));

    return box;
}

ui_interaction_t ui_button(string_t text)
{
    ui_box_t *box = ui_box(text, UI_CLICKABLE|UI_DRAW_BACKGROUND|UI_DRAW_OUTLINE|UI_DRAW_TEXT|UI_CENTER_TEXT);

    ui_set_size(box, AXIS2_X, ui_txt(1.0f));
    ui_set_size(box, AXIS2_Y, ui_txt(1.0f));

    box->hot_t    =  0.0f;
    box->active_t = -1.0f;

    return ui_interaction_from_box(box);
}

void ui_spacer(ui_size_t size)
{
    ui_box_t *spacer = ui_box(strnull, 0);
    ui_set_size(spacer, ui_layout_axis(), size);
    ui_set_size(spacer, ui_cross_axis (), ui_pct(1.0f, 0.0f));
}

ui_box_t *ui_begin_container(axis2_t layout_axis, ui_size_t size)
{
    ui_box_t *container = ui_box(strnull, 0);

    ui_set_size(container, ui_layout_axis(), size);

    container->layout_axis = layout_axis;

    ui_push_parent(container);

    return container;
}

void ui_end_container(void)
{
    ui_pop_parent();
}

ui_interaction_t ui_checkbox(string_t text, bool *toggle)
{
    ui_begin_container(AXIS2_X, ui_txt(1.0f));

    ui_box_t *button = ui_box(string_format(temp, "X##%.*s-checkbox-button", strexpand(text)), 
                              UI_CLICKABLE|UI_DRAW_BACKGROUND|UI_DRAW_OUTLINE|UI_CENTER_TEXT);
    ui_set_size(button, AXIS2_X, ui_aspect_ratio(1.0f, 1.0f));
    ui_set_size(button, AXIS2_Y, ui_pct(1.0f, 1.0f));

    ui_label(text, 0);

    ui_end_container();

    ui_interaction_t interaction = ui_interaction_from_box(button);
    if (interaction.released)
    {
        if (toggle)
            *toggle = !*toggle;
    }

    bool toggled = (toggle ? *toggle : false);

    if (toggled)
        button->flags |= UI_DRAW_TEXT;

    // button->background_color           = toggled ? g_ui.style.button_background_active_color : g_ui.style.button_background_color;
    // button->background_highlight_color = toggled ? g_ui.style.button_background_active_color : g_ui.style.button_background_highlight_color;

    return interaction;
}

void ui_increment_decrement(string_t text, int *value, int min, int max)
{
    ui_box_t *container = ui_box(string_format(temp, "%.*s##container", strexpand(text)), 0);
    ui_set_size(container, AXIS2_X, ui_pct(1.0f, 1.0f));
    ui_set_size(container, AXIS2_Y, ui_txt(1.0f));
    container->layout_axis = AXIS2_X;

    UI_Parent(container)
    {
        bool decrement = ui_button(strlit(" < ")).released;
        ui_spacer(ui_pct(1.0f, 0.0f));
        ui_label(string_format(temp, "%.*s: %d", strexpand(text), *value), UI_CENTER_TEXT);
        ui_spacer(ui_pct(1.0f, 0.0f));
        bool increment = ui_button(strlit(" > ")).released;

        if (decrement)
        {
            *value -= 1;
            if (*value < min)
                *value = min;
        }

        if (increment)
        {
            *value += 1;
            if (*value > max)
                *value = max;
        }
    }
}

static inline ui_box_t *ui_box_next_depth_first_pre_order(ui_box_t *node)
{
    ui_box_t *next = NULL;

    if (node->first)
    {
        next = node->first;
    }
    else
    {
        ui_box_t *p = node;
        
        while (p)
        {
            if (p->next)
            {
                next = p->next;
                break;
            }

            p = p->parent;
        }
    }

    return next;
}

static inline ui_box_t *ui_box_next_depth_first_post_order(ui_box_t *node)
{
    ui_box_t *next = NULL;

    if (node->first)
    {
        next = node;
        while (next->first) 
        {
            next = next->first;
        }
    }
    else
    {
        if (node->next)
        {
            next = node->next;
        }
        else
        {
            ui_box_t *p = node->parent;
            while (p)
            {
                if (p->next)
                {
                    next = p->next;
                    break;
                }

                p = p->parent;
            }
        }
    }

    return next;
}

static void ui_layout_solve_independent_sizes(ui_box_t *root, axis2_t axis)
{
    for (ui_box_t *box = root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        ui_size_t *size = &box->semantic_size[axis];
        ASSERT(size->kind != UI_SIZE_NONE);

        switch (size->kind)
        {
            case UI_SIZE_PIXELS:
            {
                box->computed_size[axis] = size->value;
            } break;

            case UI_SIZE_TEXT_CONTENT:
            {
                uint32_t font_w = g_ui.font->cw;
                uint32_t font_h = g_ui.font->ch;

                string_t display_text = box->text;

                size_t newline_count = 1;
                for (size_t i = 0; i < display_text.count; i++)
                {
                    if (i + 1 < display_text.count && display_text.data[i] == '\n')
                    {
                        newline_count += 1;
                    }
                }

                if (axis == AXIS2_X)
                {
                    box->computed_size[axis] = (float)(display_text.count*font_w);
                }
                else
                {
                    ASSERT(axis == AXIS2_Y);
                    box->computed_size[axis] = (float)newline_count*(float)font_h;
                }

                box->computed_size[axis] += 2.0f*g_ui.text_margins.e[axis];
            } break;
        }
    }
}

static void ui_layout_solve_upwards_dependent_sizes(ui_box_t *root, axis2_t axis)
{
    for (ui_box_t *box = root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        ui_size_t *size = &box->semantic_size[axis];
        ASSERT(size->kind != UI_SIZE_NONE);

        switch (size->kind)
        {
            case UI_SIZE_PERCENTAGE_OF_PARENT:
            {
                box->computed_size[axis] = box->parent->computed_size[axis]*size->value;
            } break;
        }
    }
}

static void ui_layout_solve_downwards_dependent_sizes(ui_box_t *root, axis2_t axis)
{
    for (ui_box_t *box = root; box; box = ui_box_next_depth_first_post_order(box))
    {
        ui_size_t *size = &box->semantic_size[axis];
        ASSERT(size->kind != UI_SIZE_NONE);

        switch (size->kind)
        {
            case UI_SIZE_SUM_OF_CHILDREN:
            {
                for (ui_box_t *child = box->first; child; child = child->next)
                {
                    box->computed_size[axis] += child->computed_size[axis];
                }
            } break;
        }
    }
}

static void ui_layout_solve_self_referential_sizes(ui_box_t *root, axis2_t axis)
{
    for (ui_box_t *box = root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        ui_size_t *size = &box->semantic_size[axis];
        ASSERT(size->kind != UI_SIZE_NONE);

        switch (size->kind)
        {
            case UI_SIZE_ASPECT_RATIO:
            {
                axis2_t other_axis = (axis + 1) % 2;

                ui_size_t *other_size = &box->semantic_size[other_axis];
                ASSERT(other_size->kind != UI_SIZE_ASPECT_RATIO);

                box->computed_size[axis] = size->value*box->computed_size[other_axis];
            } break;
        }
    }
}

static void ui_layout_solve_size_violations(ui_box_t *root, axis2_t axis)
{
    for (ui_box_t *box = root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        if (axis != box->layout_axis)
            continue;

        float target_size = box->computed_size[axis];

        float child_sum_strictness = 0.0f;
        float child_sum_size       = 0.0f;
        for (ui_box_t *child = box->first; child; child = child->next)
        {
            child_sum_size       += child->computed_size[axis];
            child_sum_strictness += 1.0f - child->semantic_size[axis].strictness;
        }

        float violation_size = child_sum_size - target_size;
        if (violation_size > 0.0f && child_sum_strictness > 0.01f)
        {
            for (ui_box_t *child = box->first; child; child = child->next)
            {
                float strictness_ratio = 1.0f - child->semantic_size[axis].strictness;
                child->computed_size[axis] -= violation_size*(strictness_ratio / child_sum_strictness);
            }
        }
    }
}

static void ui_layout_solve_finalize_positions(ui_box_t *root, axis2_t axis)
{
    for (ui_box_t *box = root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        box->computed_rel_position[axis] += box->position_offset[axis];

        if (box->parent)
        {
            if (box->parent->layout_axis == axis)
            {
                if (box->prev)
                {
                    box->computed_rel_position[axis] += box->prev->computed_rel_position[axis] + box->prev->computed_size[axis];
                }
            }

            if (axis == AXIS2_Y)
            {
                box->rect.min.e[axis] = box->parent->rect.max.e[axis] - box->computed_rel_position[axis] - box->computed_size[axis];
            }
            else
            {
                box->rect.min.e[axis] = box->parent->rect.min.e[axis] + box->computed_rel_position[axis];
            }
        }

        box->rect.max.e[axis] = box->rect.min.e[axis] + box->computed_size[axis];
    }
}

static void ui_animate(float dt)
{
    for (ui_box_t *box = g_ui.root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        float target_t = 0.0f;

        if (box == g_ui.hot)
            target_t = box->hot_t;

        if (box == g_ui.active)
            target_t = box->active_t;

        box->current_t += 32.0f*dt*(target_t - box->current_t);
    }
}

static void ui_solve_layout(void)
{
    ui_box_t *root = g_ui.root;

    for (int axis = 0; axis < AXIS2_COUNT; axis++)
    {
        ui_layout_solve_independent_sizes(root, axis);
        ui_layout_solve_upwards_dependent_sizes(root, axis);
        ui_layout_solve_downwards_dependent_sizes(root, axis);
    }

    for (int axis = 0; axis < AXIS2_COUNT; axis++)
    {
        ui_layout_solve_self_referential_sizes(root, axis);
    }

    for (int axis = 0; axis < AXIS2_COUNT; axis++)
    {
        ui_layout_solve_size_violations(root, axis);
        ui_layout_solve_finalize_positions(root, axis);
    }
}

static void ui_process_interactions(void)
{
    if (g_ui.active && ui_button_released(BUTTON_FIRE1))
        g_ui.active = NULL;

    ui_box_t *next_hot = NULL;

    for (ui_box_t *box = g_ui.root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        if ((box->flags & UI_CLICKABLE) && rect2_contains_point(box->rect, g_ui.mouse_p))
        {
            next_hot = box;
        }
    }

    g_ui.hot = next_hot;
}

static void ui_draw_box(ui_box_t *box, rect2_t clip_rect)
{
    clip_rect = rect2_intersect(clip_rect, box->rect);

    ui_style_t *style = &box->style;

    {
        r_immediate_draw_t *background = r_immediate_draw_begin(&(r_immediate_draw_t){
            .clip_rect = clip_rect,
        });

        if (box->flags & UI_DRAW_BACKGROUND)
        {
            ui_gradient_t gradient = style->background_color;

            if (has_flags_any(box->flags, UI_CLICKABLE|UI_DRAGGABLE) && box == g_ui.hot)
            {
                gradient = style->background_color_hot;
            }

            if (has_flags_any(box->flags, UI_CLICKABLE|UI_DRAGGABLE) && box == g_ui.active)
            {
                gradient = style->background_color_active;
            }

            r_push_rect2_filled_gradient(background, box->rect, gradient.colors);
        }

        if (box->flags & UI_DRAW_OUTLINE)
        {
            v4_t color = style->outline_color.colors[0];

            rect2_t h0 = {
                .min = { box->rect.min.x,     box->rect.min.y     },
                .max = { box->rect.max.x,     box->rect.min.y + 1 },
            };

            rect2_t h1 = {
                .min = { box->rect.min.x,     box->rect.max.y - 1 },
                .max = { box->rect.max.x,     box->rect.max.y     },
            };

            rect2_t v0 = {
                .min = { box->rect.min.x,     box->rect.min.y     },
                .max = { box->rect.min.x + 1, box->rect.max.y     },
            };

            rect2_t v1 = {
                .min = { box->rect.max.x - 1, box->rect.min.y     },
                .max = { box->rect.max.x,     box->rect.max.y     },
            };

            r_push_rect2_filled(background, h0, color);
            r_push_rect2_filled(background, h1, color);
            r_push_rect2_filled(background, v0, color);
            r_push_rect2_filled(background, v1, color);
        }

#if 0
        if (has_flags_any(box->flags, UI_CLICKABLE|UI_DRAGGABLE) && box == g_ui.active)
        {
            v4_t color = style->outline_color.colors[0];

            rect2_t h0 = {
                .min = { box->rect.min.x,     box->rect.min.y     },
                .max = { box->rect.max.x,     box->rect.min.y + 1 },
            };

            rect2_t h1 = {
                .min = { box->rect.min.x,     box->rect.max.y - 1 },
                .max = { box->rect.max.x,     box->rect.max.y     },
            };

            rect2_t v0 = {
                .min = { box->rect.min.x,     box->rect.min.y     },
                .max = { box->rect.min.x + 1, box->rect.max.y     },
            };

            rect2_t v1 = {
                .min = { box->rect.max.x - 1, box->rect.min.y     },
                .max = { box->rect.max.x,     box->rect.max.y     },
            };

            r_push_rect2_filled(background, h0, color);
            r_push_rect2_filled(background, h1, color);
            r_push_rect2_filled(background, v0, color);
            r_push_rect2_filled(background, v1, color);
        }
#endif

        r_immediate_draw_end(background);
    }

    if (RESOURCE_HANDLE_VALID(box->texture))
    {
        r_immediate_draw_t *image = r_immediate_draw_begin(&(r_immediate_draw_t){
            .clip_rect = clip_rect,
            .texture   = box->texture,
        });

        rect2_t rect = box->rect;
        if (box->flags & UI_DRAW_OUTLINE)
        {
            rect = rect2_sub_radius(rect, make_v2(1, 1));
        }

        r_push_rect2_filled(image, rect, COLORF_WHITE);

        r_immediate_draw_end(image);
    }

    if (box->text.count > 0 && has_flags_any(box->flags, UI_DRAW_TEXT))
    {
        r_immediate_draw_t *text = r_immediate_draw_begin(&(r_immediate_draw_t){
            .clip_rect = clip_rect,
            .texture   = g_ui.font->texture,
        });

        v2_t p = add(box->rect.min, g_ui.text_margins);

        string_t display_text = box->text;

        if (has_flags_any(box->flags, UI_CENTER_TEXT))
        {
            float width = (float)g_ui.font->cw*(float)display_text.count; // TODO: handle multiple lines
            p.x = 0.5f*(box->rect.min.x + box->rect.max.x) - 0.5f*width;
        }

        p = add(p, make_v2(0.0f, box->current_t));

        r_push_text(text, g_ui.font, add(p, make_v2(1, -1)), make_v4(0.0f, 0.0f, 0.0f, 0.5f), display_text);
        r_push_text(text, g_ui.font, p, style->text_color, display_text);

        r_immediate_draw_end(text);
    }

    for (ui_box_t *child = box->first; child; child = child->next)
    {
        ui_draw_box(child, clip_rect);
    }
}

static void ui_draw(void)
{
    r_push_view_screenspace();

    ui_draw_box(g_ui.root, rect2_infinity());

    r_pop_view();
}

void ui_end(float dt)
{
    ui_animate(dt);
    ui_solve_layout();
    ui_process_interactions();
    ui_draw();
}
