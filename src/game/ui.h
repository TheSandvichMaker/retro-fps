#ifndef UI_H
#define UI_H

#include "core/api_types.h"

typedef uint64_t ui_key_t;

typedef struct ui_style_t
{
    v2_t element_margins;
    v2_t text_margins;

    v4_t panel_background_color;
    v4_t button_outline_color;
    v4_t button_background_color;
    v4_t button_background_highlight_color;
    v4_t button_background_active_color;
    v4_t text_color;
} ui_style_t;

typedef enum ui_flags_t
{
    UI_CLICKABLE       = 1 << 0,
    UI_DRAGGABLE       = 1 << 1,
    UI_DRAW_BACKGROUND = 1 << 2,
    UI_DRAW_OUTLINE    = 1 << 3,
} ui_flags_t;

typedef enum ui_size_kind_t
{
    UI_SIZE_NONE,
    UI_SIZE_PIXELS,
    UI_SIZE_TEXT_CONTENT,
    UI_SIZE_PERCENTAGE_OF_PARENT,
    UI_SIZE_SUM_OF_CHILDREN,
} ui_size_kind_t;

typedef struct ui_size_t
{
    ui_size_kind_t kind;
    float value;
    float strictness;
} ui_size_t;

typedef struct ui_box_t
{
    struct ui_box_t *parent;
    struct ui_box_t *next, *prev;
    struct ui_box_t *first, *last;

    struct ui_box_t *next_in_hash;

    uint64_t last_touched_frame;
    uint32_t flags;

    axis2_t layout_axis;
    ui_size_t semantic_size[AXIS2_COUNT];

    float computed_rel_position[AXIS2_COUNT];
    float computed_size[AXIS2_COUNT];

    resource_handle_t texture;

    v4_t foreground_color;
    v4_t background_color;
    v4_t background_highlight_color;

    rect2_t rect;

    ui_key_t key;
    string_t key_string;
    string_t text;

    float hot_t;
    float active_t;

    float current_t;
} ui_box_t;

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

ui_box_t *ui_push_parent(ui_box_t *parent);
ui_box_t *ui_pop_parent(void);
ui_box_t *ui_box(string_t key, uint32_t flags);
ui_interaction_t ui_interaction_from_box(ui_box_t *box);

ui_box_t *ui_panel(string_t key, uint32_t flags, float x, float y, float w, float h);
ui_box_t *ui_label(string_t text);
ui_interaction_t ui_button(string_t text);
ui_interaction_t ui_checkbox(string_t text, bool *toggle);
// void ui_image(resource_handle_t image);
// void ui_image_scaled(resource_handle_t image, v2_t size);
// void ui_image_viewer(resource_handle_t image);

void ui_end(float dt);

#endif /* UI_H */
