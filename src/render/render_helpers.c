#include "render_helpers.h"
#include "render.h"

void r_push_line(r_immediate_draw_t *draw_call, v3_t start, v3_t end, uint32_t color)
{
    uint16_t i0 = r_immediate_vertex(draw_call, &(vertex_immediate_t){ .pos = start, .col = color });
    r_immediate_index(draw_call, i0);
    uint16_t i1 = r_immediate_vertex(draw_call, &(vertex_immediate_t){ .pos = end,   .col = color });
    r_immediate_index(draw_call, i1);
}

void r_push_rect2_filled(r_immediate_draw_t *draw_call, rect2_t rect, uint32_t color)
{
    uint16_t i0 = r_immediate_vertex(draw_call, &(vertex_immediate_t){ 
        .pos = { rect.min.x, rect.min.y, 0.0f }, 
        .tex = { 0, 0 },
        .col = color 
    });
    uint16_t i1 = r_immediate_vertex(draw_call, &(vertex_immediate_t){ 
        .pos = { rect.max.x, rect.min.y, 0.0f }, 
        .tex = { 1, 0 },
        .col = color 
    });
    uint16_t i2 = r_immediate_vertex(draw_call, &(vertex_immediate_t){ 
        .pos = { rect.max.x, rect.max.y, 0.0f }, 
        .tex = { 1, 1 },
        .col = color 
    });
    uint16_t i3 = r_immediate_vertex(draw_call, &(vertex_immediate_t){ 
        .pos = { rect.min.x, rect.max.y, 0.0f }, 
        .tex = { 0, 1 },
        .col = color 
    });

    // triangle 1
    r_immediate_index(draw_call, i0);
    r_immediate_index(draw_call, i1);
    r_immediate_index(draw_call, i2);

    // triangle 2
    r_immediate_index(draw_call, i0);
    r_immediate_index(draw_call, i2);
    r_immediate_index(draw_call, i3);
}

void r_push_arrow(r_immediate_draw_t *draw_call, v3_t start, v3_t end, uint32_t color)
{
    float head_size = 1.0f;

    v3_t  arrow_vector = sub(end, start);
    float arrow_length = vlen(arrow_vector);

    v3_t arrow_direction = normalize_or_zero(arrow_vector);

    float shaft_length = max(0.0f, arrow_length - 3.0f*head_size);

    v3_t shaft_vector = mul(shaft_length, arrow_direction);
    v3_t shaft_end    = add(start, shaft_vector);

    v3_t t, b;
    get_tangent_vectors(arrow_direction, &t, &b);

    size_t arrow_segment_count = 8;
    for (size_t i = 0; i < arrow_segment_count; i++)
    {
        float circ0 = 2.0f*PI32*((float)(i + 0) / (float)arrow_segment_count);
        float circ1 = 2.0f*PI32*((float)(i + 1) / (float)arrow_segment_count);

        float s0, c0;
        sincos_ss(circ0, &s0, &c0);

        float s1, c1;
        sincos_ss(circ1, &s1, &c1);

        v3_t v0 = add(shaft_end, add(mul(t, head_size*s0), mul(b, head_size*c0)));
        v3_t v1 = add(shaft_end, add(mul(t, head_size*s1), mul(b, head_size*c1)));
        r_push_line(draw_call, v0, v1, color);

        r_push_line(draw_call, v0, end, color);
    }

    r_push_line(draw_call, start, shaft_end, color);
}

void r_push_rect3_outline(r_immediate_draw_t *draw_call, rect3_t bounds, uint32_t color)
{
    v3_t v000 = { bounds.min.x, bounds.min.y, bounds.min.z };
    v3_t v100 = { bounds.max.x, bounds.min.y, bounds.min.z };
    v3_t v010 = { bounds.min.x, bounds.max.y, bounds.min.z };
    v3_t v110 = { bounds.max.x, bounds.max.y, bounds.min.z };

    v3_t v001 = { bounds.min.x, bounds.min.y, bounds.max.z };
    v3_t v101 = { bounds.max.x, bounds.min.y, bounds.max.z };
    v3_t v011 = { bounds.min.x, bounds.max.y, bounds.max.z };
    v3_t v111 = { bounds.max.x, bounds.max.y, bounds.max.z };

    // bottom plane
    r_push_line(draw_call, v000, v100, color);
    r_push_line(draw_call, v100, v110, color);
    r_push_line(draw_call, v110, v010, color);
    r_push_line(draw_call, v010, v000, color);

    // top plane
    r_push_line(draw_call, v001, v101, color);
    r_push_line(draw_call, v101, v111, color);
    r_push_line(draw_call, v111, v011, color);
    r_push_line(draw_call, v011, v001, color);

    // "pillars"
    r_push_line(draw_call, v000, v001, color);
    r_push_line(draw_call, v100, v101, color);
    r_push_line(draw_call, v010, v011, color);
    r_push_line(draw_call, v110, v111, color);
}

void r_draw_text(const bitmap_font_t *font, v2_t p, uint32_t color, string_t string)
{
    // this is stupid, r_immediate_* is supposed to just be putting vertices, really,
    // but r_immediate_text is a full draw call by itself that pushes a view and everything

    // unintuitive and weird.

    r_push_view_screenspace();

    r_command_identifier(string_format(temp, "text: %.*s", strexpand(string)));

    r_immediate_draw_t *draw_call = r_immediate_draw_begin(&(r_immediate_draw_t){
        .texture = font->texture,
    });

    ASSERT(font->w / font->cw == 16);
    ASSERT(font->w % font->cw ==  0);
    ASSERT(font->h / font->ch >= 16);

    v2_t at = p;

    for (size_t i = 0; i < string.count; i++)
    {
        char c = string.data[i];

        float cx = (float)(c % 16);
        float cy = (float)(c / 16);

        float cw = (float)font->cw;
        float ch = (float)font->ch;

        if (is_newline(c))
        {
            at.y += ch;
            at.x  = p.x;
        }
        else
        {
            float u0 = cx / 16.0f;
            float u1 = u0 + (1.0f / 16.0f);

            float v0 = cy / 16.0f;
            float v1 = v0 + (1.0f / 16.0f);

            uint16_t i0 = r_immediate_vertex(draw_call, &(vertex_immediate_t) {
                .pos = { at.x, at.y, 0.0f }, // TODO: Use Z?
                .tex = { u0, v1 },
                .col = color,
            });

            uint16_t i1 = r_immediate_vertex(draw_call, &(vertex_immediate_t) {
                .pos = { at.x + cw, at.y, 0.0f }, // TODO: Use Z?
                .tex = { u1, v1 },
                .col = color,
            });

            uint16_t i2 = r_immediate_vertex(draw_call, &(vertex_immediate_t) {
                .pos = { at.x + cw, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = { u1, v0 },
                .col = color,
            });

            uint16_t i3 = r_immediate_vertex(draw_call, &(vertex_immediate_t) {
                .pos = { at.x, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = { u0, v0 },
                .col = color,
            });

            r_immediate_index(draw_call, i0);
            r_immediate_index(draw_call, i1);
            r_immediate_index(draw_call, i2);
            r_immediate_index(draw_call, i0);
            r_immediate_index(draw_call, i2);
            r_immediate_index(draw_call, i3);

            at.x += cw;
        }
    }

    r_immediate_draw_end(draw_call);

    r_pop_view();
}
