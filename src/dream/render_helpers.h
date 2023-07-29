#ifndef RENDER_HELPERS_H
#define RENDER_HELPERS_H

#include "core/api_types.h"

DREAM_API void r_immediate_line                 (v3_t start, v3_t end, v4_t color);
DREAM_API void r_immediate_line_gradient        (v3_t start, v3_t end, v4_t start_color, v4_t end_color);
DREAM_API void r_immediate_arrow                (v3_t start, v3_t end, v4_t color);
DREAM_API void r_immediate_arrow_gradient       (v3_t start, v3_t end, v4_t start_color, v4_t end_color);
DREAM_API void r_immediate_rect2_filled         (rect2_t rect, v4_t color);
DREAM_API void r_immediate_rect2_filled_gradient(rect2_t rect, v4_t colors[4]);
DREAM_API void r_immediate_rect3_outline        (rect3_t bounds, v4_t color);
DREAM_API void r_immediate_triangle             (triangle_t triangle, v4_t color);
DREAM_API void r_immediate_quad                 (vertex_immediate_t a, vertex_immediate_t b, vertex_immediate_t c, vertex_immediate_t d);

DREAM_API void r_draw_text(const struct bitmap_font_t *font, v2_t p, v4_t color, string_t string);

#endif /* RENDER_HELPERS_H */
