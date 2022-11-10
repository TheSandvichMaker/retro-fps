#include "ui.h"
#include "input.h"
#include "core/arena.h"
#include "core/string.h"
#include "render/render.h"
#include "render/render_helpers.h"

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

    ui_style_t style;

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

bool ui_begin(bitmap_font_t *font, ui_style_t *style)
{
    int mouse_x, mouse_y;
    get_mouse(&mouse_x, &mouse_y);

    g_ui.mouse_p = make_v2((float)mouse_x, (float)mouse_y);
    g_ui.font = font;

    copy_struct(&g_ui.style, style);

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

    R_DRAWCALL_SCREENSPACE
    {
        r_push_rect2_filled(bounds, g_ui.style.panel_background_color);
    }
}

void ui_end_panel()
{
    if (ALWAYS(g_ui.first_panel))
    {
        sll_pop(g_ui.first_panel);
    }
}

static inline rect2_t ui_layout_horz(float height)
{
    ui_panel_t *panel = g_ui.first_panel;

    rect2_t result;
    result.min.x = panel->bounds.min.x;
    result.min.y = panel->layout_p.y - height;
    result.max.x = panel->bounds.max.x;
    result.max.y = panel->layout_p.y;

    panel->layout_p.y -= height;

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

void ui_label(string_t label)
{
    rect2_t bounds = ui_layout_horz(ui_font_height() + 2.0f*g_ui.style.text_margins.y);

    v2_t p = bounds.min;
    p.x += 2.0f*g_ui.style.text_margins.x;
    p.y += 0;//2.0f*g_ui.style.text_margins.y;
    r_draw_text(g_ui.font, p, g_ui.style.text_color, label);
}

bool ui_button(string_t label)
{
    bool result = false;

    rect2_t bounds = ui_layout_horz(ui_standard_element_height());
    bool hovered = rect2_contains_point(bounds, g_ui.mouse_p);

    if (!has_flags_any(g_ui.interaction, UI_INTERACTION_ACTIVE) && hovered)
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

    rect2_t bounds_button = rect2_sub_radius(bounds,        g_ui.style.element_margins);
    rect2_t bounds_text   = rect2_sub_radius(bounds_button, g_ui.style.text_margins);

    // probably silly
    R_DRAWCALL_SCREENSPACE
    {
        uint32_t background_color = hovered ? g_ui.style.button_background_highlight_color : 
                                              g_ui.style.button_background_color;

        r_push_rect2_filled(bounds_button, background_color);
    }

    r_draw_text(g_ui.font, bounds_text.min, g_ui.style.text_color, label);

    return result;
}
