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

typedef uint64_t ui_key_t;

typedef struct ui_element_state_t
{
    struct ui_element_state_t *next;
    ui_key_t key;

    uint64_t last_touched_frame;

    v2_t scroll;
    float zoom;
} ui_element_state_t;

typedef struct ui_state_t
{
    arena_t arena;

    uint64_t frame_index;

    ui_element_state_t *first_free_element_state;
    ui_element_state_t *element_state_hash[4096];

    ui_style_t style;

    bitmap_font_t *font;
    ui_panel_t *first_panel;

    v2_t mouse_p;
    v2_t mouse_dp;
    v2_t press_p;
    v2_t screen_dim;

    bool has_focus;
    uint32_t interaction;
    uint32_t prev_interaction;
} ui_state_t;

static ui_state_t g_ui;

bool ui_get_element_state(uint64_t key, ui_element_state_t **result)
{
    uint64_t index = key % ARRAY_COUNT(g_ui.element_state_hash); 

    ui_element_state_t *element_state = NULL;
    for (ui_element_state_t *hash_probe = g_ui.element_state_hash[index];
         hash_probe;
         hash_probe = hash_probe->next)
    {
        if (hash_probe->key == key)
        {
            element_state = hash_probe;
        }
    }

    bool new_state = !element_state;

    if (!element_state)
    {
        if (!g_ui.first_free_element_state)
        {
            g_ui.first_free_element_state = m_alloc_struct(&g_ui.arena, ui_element_state_t);
        }
        element_state = sll_pop(g_ui.first_free_element_state);
        zero_struct(element_state);

        element_state->next = g_ui.element_state_hash[index];
        g_ui.element_state_hash[index] = element_state;

        element_state->key = key;
    }

    *result = element_state;

    return new_state;
}

bool ui_begin(bitmap_font_t *font, const ui_style_t *style)
{
    int mouse_x, mouse_y;
    get_mouse(&mouse_x, &mouse_y);

    g_ui.mouse_p = make_v2((float)mouse_x, (float)mouse_y);

    int mouse_dx, mouse_dy;
    get_mouse_delta(&mouse_dx, &mouse_dy);

    g_ui.mouse_dp = make_v2((float)mouse_dx, (float)mouse_dy);

    g_ui.font = font;

    int resolution_x, resolution_y;
    render->get_resolution(&resolution_x, &resolution_y);

    g_ui.screen_dim = make_v2((float)resolution_x, (float)resolution_y);

    copy_struct(&g_ui.style, style);

    if (ui_button_pressed(BUTTON_FIRE1))
    {
        g_ui.press_p = g_ui.mouse_p;
    }

    if (has_flags_any(g_ui.interaction, UI_INTERACTION_ACTIVE))
        g_ui.has_focus = true;

    if (ui_button_pressed(BUTTON_FIRE1|BUTTON_FIRE2))
        g_ui.has_focus = has_flags_any(g_ui.interaction, UI_INTERACTION_HOVERING);

    g_ui.interaction &= ~UI_INTERACTION_HOVERING;

    g_ui.frame_index += 1;

    return g_ui.has_focus;
}

void ui_begin_panel(rect2_t bounds)
{
    ui_panel_t *panel = m_alloc_struct(temp, ui_panel_t);
    sll_push(g_ui.first_panel, panel);

    panel->bounds = bounds;
    panel->layout_p = make_v2(bounds.min.x, bounds.max.y); // start at the top, because UI probably wants to be top-down

    if (rect2_contains_point(bounds, g_ui.mouse_p))
        g_ui.interaction |= UI_INTERACTION_HOVERING;

    r_immediate_draw_t *draw_call = r_immediate_draw_begin(NULL);
    r_push_rect2_filled(draw_call, bounds, g_ui.style.panel_background_color);
    r_immediate_draw_end(draw_call);
}

void ui_end_panel()
{
    if (ALWAYS(g_ui.first_panel))
    {
        sll_pop(g_ui.first_panel);
    }
}

static inline rect2_t ui_layout_vert(float height)
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
    rect2_t bounds = ui_layout_vert(ui_font_height() + 2.0f*g_ui.style.text_margins.y);

    v2_t p = bounds.min;
    p.x += 2.0f*g_ui.style.text_margins.x;
    p.y += 0;//2.0f*g_ui.style.text_margins.y;
    r_draw_text(g_ui.font, p, g_ui.style.text_color, label);
}

bool ui_button(string_t label)
{
    bool result = false;

    rect2_t bounds = ui_layout_vert(ui_standard_element_height());
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

    uint32_t background_color = hovered ? g_ui.style.button_background_highlight_color : 
                                          g_ui.style.button_background_color;

    r_immediate_draw_t *draw_call = r_immediate_draw_begin(NULL);
    r_push_rect2_filled(draw_call, bounds_button, background_color);
    r_immediate_draw_end(draw_call);

    r_draw_text(g_ui.font, bounds_text.min, g_ui.style.text_color, label);

    return result;
}

void ui_image(resource_handle_t image)
{
    texture_desc_t desc;
    render->describe_texture(image, &desc);

    ui_image_scaled(image, make_v2((float)desc.w, (float)desc.h));
}

void ui_image_scaled(resource_handle_t image, v2_t size)
{
    rect2_t bounds = ui_layout_vert(size.y + 2.0f*g_ui.style.element_margins.y);

    rect2_t image_bounds;
    image_bounds.min = add(bounds.min, g_ui.style.element_margins);
    image_bounds.max = add(image_bounds.min, size);

    r_immediate_draw_t *draw_call = r_immediate_draw_begin(&(r_immediate_draw_t){ .texture = image });
    r_push_rect2_filled(draw_call, image_bounds, COLOR32_WHITE);
    r_immediate_draw_end(draw_call);
}

ui_key_t ui_key(string_t string)
{
    return string_hash(string);
}

void ui_image_viewer(resource_handle_t image)
{
    texture_desc_t desc;
    render->describe_texture(image, &desc);

    float height = (float)desc.h;

    rect2_t bounds = ui_layout_vert(height);

    ui_key_t key = ui_key(string_format(temp, "image:%u:%u", image.index, image.generation));

    ui_element_state_t *state;
    if (ui_get_element_state(key, &state))
    {
        state->zoom = 0.0f;
    }

    bool hovered = rect2_contains_point(bounds, g_ui.mouse_p);

    if (!has_flags_any(g_ui.interaction, UI_INTERACTION_ACTIVE) && hovered)
    {
        if (ui_button_pressed(BUTTON_FIRE1))
        {
            g_ui.interaction |= UI_INTERACTION_ACTIVE;
        }
    }

    if (has_flags_any(g_ui.interaction, UI_INTERACTION_ACTIVE) && rect2_contains_point(bounds, g_ui.press_p))
    {
        if (ui_button_down(BUTTON_FIRE1))
        {
            if (ui_button_down(BUTTON_CROUCH))
            {
                state->zoom += 0.001f*g_ui.mouse_dp.x;
                state->zoom += 0.001f*g_ui.mouse_dp.y;
            }
            else
            {
                state->scroll = add(state->scroll, g_ui.mouse_dp);
            }
        }
        else
        {
            g_ui.interaction &= ~UI_INTERACTION_ACTIVE;
        }
    }

    v2_t image_origin = add(bounds.min, g_ui.style.element_margins);
    image_origin = add(image_origin, state->scroll);

    v2_t image_size = {
        expf(state->zoom)*height*((float)desc.w / (float)desc.h),
        expf(state->zoom)*height,
    };

    rect2_t image_bounds;
    image_bounds.min = image_origin;
    image_bounds.max = add(image_origin, image_size);

    r_immediate_draw_t *draw_call = r_immediate_draw_begin(&(r_immediate_draw_t){ 
        .texture   = image,
        .clip_rect = bounds,
    });
    r_push_rect2_filled(draw_call, image_bounds, COLOR32_WHITE);
    r_immediate_draw_end(draw_call);

    r_draw_text(g_ui.font, bounds.min, COLOR32_RED, string_format(temp, "zoom: %.02f", state->zoom));
}
