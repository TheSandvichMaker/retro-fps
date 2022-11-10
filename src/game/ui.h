#ifndef UI_H
#define UI_H

#include "core/api_types.h"

typedef struct ui_style_t
{
    v2_t element_margins;
    v2_t text_margins;

    uint32_t panel_background_color;
    uint32_t button_background_color;
    uint32_t button_background_highlight_color;
    uint32_t text_color;
} ui_style_t;

bool ui_begin(struct bitmap_font_t *font, ui_style_t *style);

void ui_begin_panel(rect2_t bounds);
void ui_end_panel(void);
void ui_label(string_t label);
bool ui_button(string_t label);

void ui_end(void);

#endif /* UI_H */
