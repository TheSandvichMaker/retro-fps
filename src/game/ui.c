#include "ui.h"
#include "input.h"
#include "core/arena.h"
#include "core/string.h"
#include "render/render.h"
#include "render/render_helpers.h"

static inline string_t ui_display_text(string_t key_string)
{
    string_t result = substring(key_string, 0, string_find_first(key_string, strlit("##")));
    return result;
}

static inline ui_key_t ui_key(string_t string)
{
    return string_hash(string);
}

typedef struct ui_state_t
{
    arena_t frame_arena;
    arena_t arena;

    uint64_t frame_index;

    ui_box_t *first_free_box;
    ui_box_t *box_hash[4096];

    ui_box_t *root;
    ui_box_t *current_parent;

    ui_style_t style;

    bitmap_font_t *font;

    ui_box_t *hot;
    ui_box_t *active;

    v2_t mouse_p;
    v2_t mouse_dp;
    v2_t press_p;
    v2_t screen_dim;

    bool has_focus;
} ui_state_t;

static ui_state_t g_ui;

ui_box_t *ui_get_or_create_box(string_t key_string)
{
    ui_key_t key   = ui_key(key_string);
    uint64_t index = key % ARRAY_COUNT(g_ui.box_hash); 

    ui_box_t *box = NULL;
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

    if (!box)
    {
        if (!g_ui.first_free_box)
        {
            g_ui.first_free_box = m_alloc_struct(&g_ui.arena, ui_box_t);
        }
        box = sll_pop(g_ui.first_free_box);
        zero_struct(box);

        box->next_in_hash = g_ui.box_hash[index];
        g_ui.box_hash[index] = box;

        box->key = key;
    }

    box->key_string = string_copy(&g_ui.frame_arena, key_string);

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

    if (box->parent)
        dll_push_back(box->parent->first, box->parent->last, box);

    ASSERT(box->first != box);

    box->last_touched_frame = g_ui.frame_index;

    return box;
}

bool ui_begin(bitmap_font_t *font, const ui_style_t *style)
{
    m_reset(&g_ui.frame_arena);

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

    g_ui.current_parent = NULL;

    g_ui.root = ui_get_or_create_box(strlit("root"));
    g_ui.current_parent = g_ui.root;

    g_ui.root->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_SUM_OF_CHILDREN, 0.0f, 1.0f };
    g_ui.root->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_SUM_OF_CHILDREN, 0.0f, 1.0f };

    int mouse_x, mouse_y;
    get_mouse(&mouse_x, &mouse_y);

    g_ui.mouse_p = make_v2((float)mouse_x, (float)mouse_y);

    if (ui_button_pressed(BUTTON_FIRE1))
        g_ui.press_p = g_ui.mouse_p;

    int mouse_dx, mouse_dy;
    get_mouse_delta(&mouse_dx, &mouse_dy);

    g_ui.mouse_dp = make_v2((float)mouse_dx, (float)mouse_dy);

    g_ui.font = font;

    int resolution_x, resolution_y;
    render->get_resolution(&resolution_x, &resolution_y);

    g_ui.screen_dim = make_v2((float)resolution_x, (float)resolution_y);

    copy_struct(&g_ui.style, style);

    g_ui.frame_index += 1;

    if (ui_button_pressed(BUTTON_FIRE1|BUTTON_FIRE2))
    {
        if (g_ui.hot)
        {
            g_ui.active = g_ui.hot;

            g_ui.has_focus = true;
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
        result.mouse         = g_ui.mouse_p;
        result.drag_delta    = g_ui.mouse_dp;
        result.clicked       = ui_button_pressed(BUTTON_FIRE1);
        result.right_clicked = ui_button_pressed(BUTTON_FIRE2);
        result.hovering      = true;
    }

    if (box == g_ui.active)
    {
        result.dragging = !!(box->flags & UI_DRAGGABLE);
        result.released = ui_button_released(BUTTON_FIRE1);
    }

    return result;
}

ui_box_t *ui_box(string_t key, uint32_t flags)
{
    ui_box_t *box = ui_get_or_create_box(key);

    box->flags            = flags;
    box->layout_axis      = AXIS2_Y;
    box->foreground_color = g_ui.style.text_color;
    box->background_color = g_ui.style.panel_background_color;

    return box;
}

ui_box_t *ui_panel(string_t key, uint32_t flags, float x, float y, float w, float h)
{
    ui_box_t *box = ui_box(string_format(temp, "%.*s##panel", strexpand(key)), UI_DRAW_BACKGROUND|flags);

    box->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PIXELS, w, 1.0f };
    box->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_PIXELS, h, 1.0f };
    box->rect.min = make_v2(x, y);

    return box;
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
    return (float)g_ui.font->ch + 2.0f*g_ui.style.element_margins.y + 2.0f*g_ui.style.text_margins.y;
}

ui_box_t *ui_label(string_t text)
{
    ui_box_t *box = ui_box(text, 0);

    box->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PERCENTAGE_OF_PARENT, 1.0f, 1.0f };
    box->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_TEXT_CONTENT,         1.0f, 1.0f };
    box->text = string_copy(&g_ui.frame_arena, text);

    return box;
}

ui_interaction_t ui_button(string_t text)
{
    ui_box_t *box = ui_box(text, UI_CLICKABLE|UI_DRAW_BACKGROUND|UI_DRAW_OUTLINE);

    box->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PERCENTAGE_OF_PARENT, 1.0f, 1.0f };
    box->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_TEXT_CONTENT,         1.0f, 1.0f };
    box->background_color           = g_ui.style.button_background_color;
    box->background_highlight_color = g_ui.style.button_background_highlight_color;
    box->text = string_copy(&g_ui.frame_arena, text);

    box->hot_t    =  0.0f;
    box->active_t = -1.0f;

    return ui_interaction_from_box(box);
}

ui_interaction_t ui_checkbox(string_t text, bool *toggle)
{
    ui_box_t *container = ui_box(string_format(temp, "%.*s##container", strexpand(text)), 0);
    container->layout_axis = AXIS2_X;
    container->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PERCENTAGE_OF_PARENT, 1.0f, 1.0f };
    container->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_TEXT_CONTENT,         1.0f, 1.0f };

    ui_push_parent(container);

    ui_box_t *label = ui_box(text, UI_DRAW_OUTLINE);
    label->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PERCENTAGE_OF_PARENT, 0.9f, 1.0f };
    label->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_PERCENTAGE_OF_PARENT, 1.0f, 1.0f };
    label->text = string_copy(&g_ui.frame_arena, text);

    ui_box_t *button = ui_box(string_format(temp, "%.*s##button", strexpand(text)), UI_CLICKABLE|UI_DRAW_BACKGROUND|UI_DRAW_OUTLINE);
    button->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PERCENTAGE_OF_PARENT, 0.1f, 1.0f };
    button->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_PERCENTAGE_OF_PARENT, 1.0f, 1.0f };

    ui_pop_parent();

    ui_interaction_t interaction = ui_interaction_from_box(button);
    if (interaction.clicked)
    {
        if (toggle)
            *toggle = !*toggle;
    }

    bool toggled = (toggle ? *toggle : false);

    button->background_color           = toggled ? g_ui.style.button_background_active_color : g_ui.style.button_background_color;
    button->background_highlight_color = toggled ? g_ui.style.button_background_active_color : g_ui.style.button_background_highlight_color;

    return interaction;
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

                string_t display_text = ui_display_text(box->text);

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

                box->computed_size[axis] += 2.0f*g_ui.style.text_margins.e[axis];
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

static void ui_layout_solve_size_violations(ui_box_t *root, axis2_t axis)
{
    (void)root;
    (void)axis;
}

static void ui_layout_solve_finalize_positions(ui_box_t *root, axis2_t axis)
{
    for (ui_box_t *box = root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        if (box->parent)
        {
            if (box->parent->layout_axis == axis)
            {
                if (box->prev)
                {
                    box->computed_rel_position[axis] = box->prev->computed_rel_position[axis] + box->prev->computed_size[axis];
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
        if (rect2_contains_point(box->rect, g_ui.mouse_p))
        {
            next_hot = box;
        }
    }

    g_ui.hot = next_hot;
}

static void ui_draw(void)
{
    r_push_view_screenspace();

    for (ui_box_t *box = g_ui.root; box; box = ui_box_next_depth_first_pre_order(box))
    {
        r_immediate_draw_t *background = r_immediate_draw_begin(&(r_immediate_draw_t){
            .clip_rect = box->rect,
        });

        if (box->flags & UI_DRAW_BACKGROUND)
        {
            v4_t color = box->background_color;

            if (has_flags_any(box->flags, UI_CLICKABLE|UI_DRAGGABLE) &&
                (box == g_ui.hot || box == g_ui.active))
            {
                color = box->background_highlight_color;
            }

            v4_t color_bottom = color;
            color_bottom.xyz = mul(0.65f, color_bottom.xyz);
            
            rect2_t rect = box->rect;

            uint32_t i0 = r_immediate_vertex(background, &(vertex_immediate_t){ 
                .pos = { rect.min.x, rect.min.y, 0.0f }, 
                .tex = { 0, 0 },
                .col = pack_color(color_bottom)
            });
            uint32_t i1 = r_immediate_vertex(background, &(vertex_immediate_t){ 
                .pos = { rect.max.x, rect.min.y, 0.0f }, 
                .tex = { 1, 0 },
                .col = pack_color(color_bottom)
            });
            uint32_t i2 = r_immediate_vertex(background, &(vertex_immediate_t){ 
                .pos = { rect.max.x, rect.max.y, 0.0f }, 
                .tex = { 1, 1 },
                .col = pack_color(color) 
            });
            uint32_t i3 = r_immediate_vertex(background, &(vertex_immediate_t){ 
                .pos = { rect.min.x, rect.max.y, 0.0f }, 
                .tex = { 0, 1 },
                .col = pack_color(color) 
            });

            // triangle 1
            r_immediate_index(background, i0);
            r_immediate_index(background, i1);
            r_immediate_index(background, i2);

            // triangle 2
            r_immediate_index(background, i0);
            r_immediate_index(background, i2);
            r_immediate_index(background, i3);
        }

        if (box->flags & UI_DRAW_OUTLINE)
        {
            v4_t color = g_ui.style.button_outline_color;

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

            r_push_rect2_filled(background, h0, pack_color(color));
            r_push_rect2_filled(background, h1, pack_color(color));
            r_push_rect2_filled(background, v0, pack_color(color));
            r_push_rect2_filled(background, v1, pack_color(color));
        }

        r_immediate_draw_end(background);

        if (RESOURCE_HANDLE_VALID(box->texture))
        {
            r_immediate_draw_t *image = r_immediate_draw_begin(&(r_immediate_draw_t){
                .clip_rect = box->rect,
                .texture   = box->texture,
            });

            rect2_t rect = box->rect;
            if (box->flags & UI_DRAW_OUTLINE)
            {
                rect = rect2_sub_radius(rect, make_v2(1, 1));
            }

            r_push_rect2_filled(background, rect, COLOR32_WHITE);

            r_immediate_draw_end(image);
        }

        if (box->text.count > 0)
        {
            r_immediate_draw_t *text = r_immediate_draw_begin(&(r_immediate_draw_t){
                .clip_rect = box->rect,
                .texture   = g_ui.font->texture,
            });

            v2_t p = add(box->rect.min, g_ui.style.text_margins);

            p = add(p, make_v2(0.0f, box->current_t));

            string_t display_text = ui_display_text(box->text);

            r_push_text(text, g_ui.font, add(p, make_v2(1, -1)), pack_rgba(0.0f, 0.0f, 0.0f, 0.5f), display_text);
            r_push_text(text, g_ui.font, p, pack_color(box->foreground_color), display_text);

            r_immediate_draw_end(text);
        }
    }

    r_pop_view();
}

void ui_end(float dt)
{
    ui_animate(dt);
    ui_solve_layout();
    ui_process_interactions();
    ui_draw();
}
