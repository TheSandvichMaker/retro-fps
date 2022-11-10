#include "ui.h"
#include "input.h"
#include "core/arena.h"
#include "core/string.h"
#include "render/render.h"

typedef enum ui_interaction_t
{
    UI_INTERACTION_NONE,
    UI_INTERACTION_HOVERING = 1 << 0,
    UI_INTERACTION_ACTIVE   = 1 << 1,
} ui_interaction_t;

typedef struct ui_panel_t
{
    struct ui_panel_t *next;
    rect2_t bounds;
    v2_t layout_p;
} ui_panel_t;

typedef struct ui_state_t
{
    arena_t arena;

    bitmap_font_t *font;
    ui_panel_t *first_panel;

    v2_t mouse_p;
    v2_t press_p;
    v2_t screen_dim;

    bool has_focus;
    uint32_t interaction;
    uint32_t prev_interaction;
} ui_state_t;

static ui_state_t g_ui;

bool ui_begin(bitmap_font_t *font)
{
    int mouse_x, mouse_y;
    get_mouse(&mouse_x, &mouse_y);

    g_ui.mouse_p = make_v2((float)mouse_x, (float)mouse_y);
    g_ui.font = font;

    if (ui_button_pressed(BUTTON_FIRE1))
    {
        g_ui.press_p = g_ui.mouse_p;
    }

    return g_ui.has_focus;
}

void ui_end(void)
{
    if (has_flags_any(g_ui.interaction, UI_INTERACTION_ACTIVE))
        g_ui.has_focus = true;

    if (ui_button_pressed(BUTTON_FIRE1|BUTTON_FIRE2))
        g_ui.has_focus = has_flags_any(g_ui.interaction, UI_INTERACTION_HOVERING);

    g_ui.interaction &= ~UI_INTERACTION_HOVERING;

    m_reset(&g_ui.arena);
}

void ui_begin_panel(rect2_t bounds)
{
    ui_panel_t *panel = m_alloc_struct(&g_ui.arena, ui_panel_t);
    sll_push(g_ui.first_panel, panel);

    panel->bounds = bounds;
    panel->layout_p = make_v2(bounds.min.x, bounds.max.y); // start at the top, because UI probably wants to be top-down

    if (rect2_contains_point(bounds, g_ui.mouse_p))
        g_ui.interaction |= UI_INTERACTION_HOVERING;

    // FIXME: horrible bad. stupid.
    r_immediate_depth_test(false);
    r_push_view_screenspace();
    r_command_identifier(strlit("the panel"));
    r_immediate_filled_rect2(bounds, pack_rgba(0.015f, 0.015f, 0.015f, 1.0f));
    r_immediate_flush();
    r_pop_view();
}

void ui_end_panel()
{
    if (ALWAYS(g_ui.first_panel))
    {
        sll_pop(g_ui.first_panel);
    }
}

rect2_t ui_layout_horz(float height)
{
    ui_panel_t *panel = g_ui.first_panel;

    rect2_t result;
    result.min.x = panel->bounds.min.x;
    result.max.x = panel->bounds.max.x;
    result.min.y = panel->layout_p.y - height;
    result.max.y = panel->layout_p.y;

    panel->layout_p.y -= height;

    return result;
}

bool ui_button(string_t label)
{
    bool result = false;

    float button_outer_margin = 4.0f;
    float button_inner_margin = 4.0f;
    float button_height = 2.0f*button_outer_margin + 2.0f*button_inner_margin + (float)g_ui.font->ch;

    rect2_t bounds = ui_layout_horz(button_height);

    if (!has_flags_any(g_ui.interaction, UI_INTERACTION_ACTIVE) && 
        rect2_contains_point(bounds, g_ui.mouse_p))
    {
        g_ui.interaction |= UI_INTERACTION_HOVERING;

        if (ui_button_pressed(BUTTON_FIRE1))
            g_ui.interaction |= UI_INTERACTION_ACTIVE;
    }

    if (ui_button_released(BUTTON_FIRE1))
    {
        if (has_flags_any(g_ui.interaction, UI_INTERACTION_ACTIVE))
        {
            if (rect2_contains_point(bounds, g_ui.press_p))
            {
                g_ui.interaction &= ~UI_INTERACTION_ACTIVE;
                result = true;
            }
        }
    }

    rect2_t button_background_bounds = bounds;
    button_background_bounds.min.x += button_outer_margin;
    button_background_bounds.min.y += button_outer_margin;
    button_background_bounds.max.x -= button_outer_margin;
    button_background_bounds.max.y -= button_outer_margin;

    // well, something to fix later
    // FIXME: horrible bad. stupid.
    r_immediate_depth_test(false);
    r_push_view_screenspace();
    r_immediate_filled_rect2(button_background_bounds, pack_rgb(0.125f, 0.125f, 0.125f));
    r_immediate_flush();
    r_pop_view();

    r_immediate_text(g_ui.font, 
                     make_v2(button_background_bounds.min.x + button_inner_margin, 
                             button_background_bounds.min.y + button_inner_margin),
                     COLOR32_WHITE,
                     label);

    return result;
}
