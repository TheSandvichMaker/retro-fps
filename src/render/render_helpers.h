#ifndef RENDER_HELPERS_H
#define RENDER_HELPERS_H

#include "core/api_types.h"

//
// These are functions that let you easily draw various shapes and such using the
// immediate mode rendering API
//
// This is separate from render.h, because it's a different "layer" of functionality.
//

// r_push_* functions only put vertices, it is up to you to set the right primitive topology
void r_push_line         (struct r_immediate_draw_t *draw_call, v3_t start, v3_t end, v4_t color);
void r_push_line_gradient(struct r_immediate_draw_t *draw_call, v3_t start, v3_t end, v4_t start_color, v4_t end_color);
void r_push_arrow         (struct r_immediate_draw_t *draw_call, v3_t start, v3_t end, v4_t color);
void r_push_arrow_gradient(struct r_immediate_draw_t *draw_call, v3_t start, v3_t end, v4_t start_color, v4_t end_color);
void r_push_rect2_filled (struct r_immediate_draw_t *draw_call, rect2_t rect, v4_t color);
void r_push_rect2_filled_gradient(struct r_immediate_draw_t *draw_call, rect2_t rect, v4_t colors[4]);
void r_push_rect3_outline(struct r_immediate_draw_t *draw_call, rect3_t bounds, v4_t color);

// r_draw_* functions do a full draw call
void r_push_text(struct r_immediate_draw_t *draw_call, const struct bitmap_font_t *font, v2_t p, v4_t color, string_t string);

#endif /* RENDER_HELPERS_H */
