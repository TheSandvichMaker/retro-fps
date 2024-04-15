#ifndef RENDER_HELPERS_H
#define RENDER_HELPERS_H

#include "core/api_types.h"
#include "render.h"

// all r_immediate_* calls just push the vertices/indices, they don't dispatch a draw call. it's up to you to make sure
// the immediate draw call is set up correctly
fn void r_immediate_line                 (r_context_t *rc, v3_t start, v3_t end, v4_t color);
fn void r_immediate_line_gradient        (r_context_t *rc, v3_t start, v3_t end, v4_t start_color, v4_t end_color);
fn void r_immediate_arrow                (r_context_t *rc, v3_t start, v3_t end, v4_t color);
fn void r_immediate_arrow_gradient       (r_context_t *rc, v3_t start, v3_t end, v4_t start_color, v4_t end_color);
fn void r_immediate_rect2_filled         (r_context_t *rc, rect2_t rect, v4_t color);
fn void r_immediate_rect2_filled_gradient(r_context_t *rc, rect2_t rect, v4_t colors[4]);
fn void r_immediate_rect3_outline        (r_context_t *rc, rect3_t bounds, v4_t color);
fn void r_immediate_itriangle            (r_context_t *rc, uint32_t a, uint32_t b, uint32_t c);
fn void r_immediate_iquad                (r_context_t *rc, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
fn void r_immediate_triangle             (r_context_t *rc, triangle_t triangle, v4_t color);
fn void r_immediate_quad                 (r_context_t *rc, r_vertex_immediate_t a, r_vertex_immediate_t b, r_vertex_immediate_t c, r_vertex_immediate_t d);
fn void r_immediate_sphere               (r_context_t *rc, v3_t p, float r, v4_t color, size_t slices, size_t stacks);
fn void r_immediate_text                 (r_context_t *rc, const struct bitmap_font_t *font, v2_t p, v4_t color, string_t string);

fn void make_quad_vertices(v3_t pos, v2_t size, quat_t rotation, v4_t color,
									r_vertex_immediate_t *a,
									r_vertex_immediate_t *b,
									r_vertex_immediate_t *c,
									r_vertex_immediate_t *d);

// This one actually does a draw call by itself
fn void r_draw_text(r_context_t *rc, const struct bitmap_font_t *font, v2_t p, v4_t color, string_t string);

#endif /* RENDER_HELPERS_H */
