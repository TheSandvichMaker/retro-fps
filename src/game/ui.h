#ifndef UI_H
#define UI_H

#include "core/api_types.h"

typedef uint64_t ui_key_t;

typedef enum ui_flags_t
{
    UI_CLICKABLE       = 1 << 0,
    UI_DRAGGABLE       = 1 << 1,
    UI_DRAW_BACKGROUND = 1 << 2,
    UI_DRAW_OUTLINE    = 1 << 3,
    UI_DRAW_TEXT       = 1 << 4,
    UI_CENTER_TEXT     = 1 << 5,
} ui_flags_t;

typedef enum ui_size_kind_t
{
    UI_SIZE_NONE,
    UI_SIZE_PIXELS,
    UI_SIZE_TEXT_CONTENT,
    UI_SIZE_PERCENTAGE_OF_PARENT,
    UI_SIZE_SUM_OF_CHILDREN,
    UI_SIZE_ASPECT_RATIO,
} ui_size_kind_t;

typedef struct ui_size_t
{
    ui_size_kind_t kind;
    float value;
    float strictness;
} ui_size_t;

static inline ui_size_t ui_pct(float value, float strictness)
{
    ui_size_t result = { UI_SIZE_PERCENTAGE_OF_PARENT, value, strictness };
    return result;
}

static inline ui_size_t ui_aspect_ratio(float value, float strictness)
{
    ui_size_t result = { UI_SIZE_ASPECT_RATIO, value, strictness };
    return result;
}

static inline ui_size_t ui_pixels(float value, float strictness)
{
    ui_size_t result = { UI_SIZE_PIXELS, value, strictness };
    return result;
}

static inline ui_size_t ui_txt(float strictness)
{
    ui_size_t result = { UI_SIZE_TEXT_CONTENT, 0.0f, strictness };
    return result;
}

typedef struct ui_gradient_t
{
    // bottom left, bottom right, top left, top right
    v4_t colors[4];
} ui_gradient_t;

static inline ui_gradient_t ui_gradient_from_rgba(float r, float g, float b, float a)
{
    ui_gradient_t result = {
        .colors = {
            { r, g, b, a },
            { r, g, b, a },
            { r, g, b, a },
            { r, g, b, a },
        },
    };
    return result;
}

static inline ui_gradient_t ui_gradient_from_v4(v4_t color)
{
    ui_gradient_t result = {
        .colors = {
            color,
            color,
            color,
            color,
        },
    };
    return result;
}

static inline ui_gradient_t ui_gradient_vertical(v4_t top, v4_t bottom)
{
    ui_gradient_t result = {
        .colors = {
            bottom,
            bottom,
            top,
            top,
        },
    };
    return result;
}

static inline ui_gradient_t ui_gradient_horizontal(v4_t left, v4_t right)
{
    ui_gradient_t result = {
        .colors = {
            left,
            right,
            left,
            right,
        },
    };
    return result;
}

typedef struct ui_style_t
{
    v4_t text_color;

    ui_gradient_t outline_color;

    ui_gradient_t background_color;
    ui_gradient_t background_color_hot;
    ui_gradient_t background_color_active;
} ui_style_t;

typedef struct ui_size_node_t
{
    struct ui_size_node_t *next;
    ui_size_t semantic_size[AXIS2_COUNT];
} ui_size_node_t;

typedef struct ui_style_node_t
{
    struct ui_style_node_t *next;
    ui_style_t style;
} ui_style_node_t;

typedef struct ui_box_t
{
    struct ui_box_t *parent;
    struct ui_box_t *next, *prev;
    struct ui_box_t *first, *last;

    struct ui_box_t *next_in_hash;

    uint64_t debug_ordinal;

    uint64_t last_touched_frame;
    uint32_t flags;

    axis2_t layout_axis;
    ui_size_t semantic_size[AXIS2_COUNT];

    float position_offset[AXIS2_COUNT];
    float computed_rel_position[AXIS2_COUNT];
    float computed_size[AXIS2_COUNT];

    resource_handle_t texture;

    ui_style_t style;

    rect2_t rect;

    ui_key_t key;
    string_t key_string;
    string_t text;

    float hot_t;
    float active_t;

    float current_t;
} ui_box_t;

static inline void ui_set_size(ui_box_t *box, axis2_t axis, ui_size_t size)
{
    box->semantic_size[axis] = size;
}

typedef struct ui_interaction_t
{
    ui_box_t *box;
    v2_t mouse;
    v2_t drag_delta;
    bool clicked;
    bool double_clicked;
    bool right_clicked;
    bool released;
    bool dragging;
    bool hovering;
} ui_interaction_t;

bool ui_begin(struct bitmap_font_t *font, const ui_style_t *style);

ui_style_t ui_get_style(void);
void ui_push_style(const ui_style_t *style);
void ui_pop_style(void);

void ui_push_background_color(ui_gradient_t color);

ui_box_t *ui_push_parent(ui_box_t *parent);
ui_box_t *ui_pop_parent(void);
ui_box_t *ui_box(string_t key, uint32_t flags);
ui_interaction_t ui_interaction_from_box(ui_box_t *box);

void ui_spacer(ui_size_t size);

ui_box_t *ui_window_begin(string_t key, uint32_t flags, float x, float y, float w, float h);
void ui_window_end(void);

#define UI_Window(key, flags, x, y, w, h) DeferLoop(ui_window_begin(key, flags, x, y, w, h), ui_window_end())
#define UI_BackgroundColor(gradient) DeferLoop(ui_push_background_color(gradient), ui_pop_style())
#define UI_Parent(parent) DeferLoop(ui_push_parent(parent), ui_pop_parent())

ui_box_t *ui_label(string_t text, uint32_t flags);
ui_interaction_t ui_button(string_t text);
ui_interaction_t ui_checkbox(string_t text, bool *toggle);
// void ui_image(resource_handle_t image);
// void ui_image_scaled(resource_handle_t image, v2_t size);
// void ui_image_viewer(resource_handle_t image);

void ui_end(float dt);

#endif /* UI_H */
