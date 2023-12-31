#ifndef RENDER_HELPERS_H
#define RENDER_HELPERS_H

#include "core/api_types.h"
#include "render.h"

DREAM_LOCAL void r_immediate_line                 (r_context_t *rc, v3_t start, v3_t end, v4_t color);
DREAM_LOCAL void r_immediate_line_gradient        (r_context_t *rc, v3_t start, v3_t end, v4_t start_color, v4_t end_color);
DREAM_LOCAL void r_immediate_arrow                (r_context_t *rc, v3_t start, v3_t end, v4_t color);
DREAM_LOCAL void r_immediate_arrow_gradient       (r_context_t *rc, v3_t start, v3_t end, v4_t start_color, v4_t end_color);
DREAM_LOCAL void r_immediate_rect2_filled         (r_context_t *rc, rect2_t rect, v4_t color);
DREAM_LOCAL void r_immediate_rect2_filled_gradient(r_context_t *rc, rect2_t rect, v4_t colors[4]);
DREAM_LOCAL void r_immediate_rect3_outline        (r_context_t *rc, rect3_t bounds, v4_t color);
DREAM_LOCAL void r_immediate_itriangle            (r_context_t *rc, uint32_t a, uint32_t b, uint32_t c);
DREAM_LOCAL void r_immediate_iquad                (r_context_t *rc, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
DREAM_LOCAL void r_immediate_triangle             (r_context_t *rc, triangle_t triangle, v4_t color);
DREAM_LOCAL void r_immediate_quad                 (r_context_t *rc, r_vertex_immediate_t a, r_vertex_immediate_t b, r_vertex_immediate_t c, r_vertex_immediate_t d);
DREAM_LOCAL void r_immediate_sphere(r_context_t *rc, v3_t p, float r, v4_t color, size_t slices, size_t stacks);

DREAM_LOCAL void r_draw_text(r_context_t *rc, const struct bitmap_font_t *font, v2_t p, v4_t color, string_t string);

#endif /* RENDER_HELPERS_H */
