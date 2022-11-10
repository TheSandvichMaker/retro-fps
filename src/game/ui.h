#ifndef UI_H
#define UI_H

#include "core/api_types.h"

bool ui_begin(struct bitmap_font_t *font);

void ui_begin_panel(rect2_t bounds);
void ui_end_panel(void);
bool ui_button(string_t label);

void ui_end(void);

#endif /* UI_H */
