#include "render_helpers.h"
#include "render.h"

void r_immediate_line(v3_t start, v3_t end, v4_t color)
{
    uint32_t color_packed = pack_color(color);

    uint32_t i0 = r_immediate_vertex(&(vertex_immediate_t){ .pos = start, .col = color_packed });
    r_immediate_index(i0);
    uint32_t i1 = r_immediate_vertex(&(vertex_immediate_t){ .pos = end,   .col = color_packed });
    r_immediate_index(i1);
}

void r_immediate_line_gradient(v3_t start, v3_t end, v4_t start_color, v4_t end_color)
{
    uint32_t i0 = r_immediate_vertex(&(vertex_immediate_t){ .pos = start, .col = pack_color(start_color) });
    r_immediate_index(i0);
    uint32_t i1 = r_immediate_vertex(&(vertex_immediate_t){ .pos = end,   .col = pack_color(end_color) });
    r_immediate_index(i1);
}

void r_immediate_rect2_filled(rect2_t rect, v4_t color)
{
    uint32_t color_packed = pack_color(color);

    uint32_t i0 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.min.x, rect.min.y, 0.0f }, 
        .tex = { 0, 0 },
        .col = color_packed,
    });
    uint32_t i1 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.max.x, rect.min.y, 0.0f }, 
        .tex = { 1, 0 },
        .col = color_packed,
    });
    uint32_t i2 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.max.x, rect.max.y, 0.0f }, 
        .tex = { 1, 1 },
        .col = color_packed,
    });
    uint32_t i3 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.min.x, rect.max.y, 0.0f }, 
        .tex = { 0, 1 },
        .col = color_packed,
    });

    // triangle 1
    r_immediate_index(i0);
    r_immediate_index(i1);
    r_immediate_index(i2);

    // triangle 2
    r_immediate_index(i0);
    r_immediate_index(i2);
    r_immediate_index(i3);
}

void r_immediate_rect2_filled_gradient(rect2_t rect, v4_t colors[4])
{
    uint32_t i0 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.min.x, rect.min.y, 0.0f }, 
        .tex = { 0, 0 },
        .col = pack_color(colors[0]),
    });
    uint32_t i1 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.max.x, rect.min.y, 0.0f }, 
        .tex = { 1, 0 },
        .col = pack_color(colors[1]), 
    });
    uint32_t i2 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.max.x, rect.max.y, 0.0f }, 
        .tex = { 1, 1 },
        .col = pack_color(colors[2]),
    });
    uint32_t i3 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos = { rect.min.x, rect.max.y, 0.0f }, 
        .tex = { 0, 1 },
        .col = pack_color(colors[3]), 
    });

    // triangle 1
    r_immediate_index(i0);
    r_immediate_index(i1);
    r_immediate_index(i2);

    // triangle 2
    r_immediate_index(i0);
    r_immediate_index(i2);
    r_immediate_index(i3);
}

void r_immediate_arrow_gradient(v3_t start, v3_t end, v4_t start_color, v4_t end_color)
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
        r_immediate_line_gradient(v0, v1, end_color, end_color);

        r_immediate_line_gradient(v0, end, end_color, end_color);
    }

    r_immediate_line_gradient(start, shaft_end, start_color, end_color);
}

void r_immediate_arrow(v3_t start, v3_t end, v4_t color)
{
    r_immediate_arrow_gradient(start, end, color, color);
}

void r_immediate_rect3_outline(rect3_t bounds, v4_t color)
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
    r_immediate_line(v000, v100, color);
    r_immediate_line(v100, v110, color);
    r_immediate_line(v110, v010, color);
    r_immediate_line(v010, v000, color);

    // top plane
    r_immediate_line(v001, v101, color);
    r_immediate_line(v101, v111, color);
    r_immediate_line(v111, v011, color);
    r_immediate_line(v011, v001, color);

    // "pillars"
    r_immediate_line(v000, v001, color);
    r_immediate_line(v100, v101, color);
    r_immediate_line(v010, v011, color);
    r_immediate_line(v110, v111, color);
}

void r_immediate_triangle(triangle_t t, v4_t color)
{
    uint32_t color_packed = pack_color(color);

	v3_t normal = triangle_normal(t.a, t.b, t.c);

    uint32_t i0 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos    = t.a,
        .col    = color_packed,
		.normal = normal,
    });

    uint32_t i1 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos    = t.b,
        .col    = color_packed,
		.normal = normal,
    });

    uint32_t i2 = r_immediate_vertex(&(vertex_immediate_t){ 
        .pos    = t.c,
        .col    = color_packed,
		.normal = normal,
    });

	r_immediate_index(i0);
	r_immediate_index(i1);
	r_immediate_index(i2);
}

void r_immediate_quad(vertex_immediate_t a, vertex_immediate_t b, vertex_immediate_t c, vertex_immediate_t d)
{
    uint32_t i0 = r_immediate_vertex(&a);
    uint32_t i1 = r_immediate_vertex(&b);
    uint32_t i2 = r_immediate_vertex(&c);
    uint32_t i3 = r_immediate_vertex(&d);

	r_immediate_index(i0);
	r_immediate_index(i1);
	r_immediate_index(i2);
	r_immediate_index(i0);
	r_immediate_index(i2);
	r_immediate_index(i3);
}

void r_draw_text(const bitmap_font_t *font, v2_t p, v4_t color, string_t string)
{
	r_immediate_texture(font->texture);

    ASSERT(font->w / font->cw == 16);
    ASSERT(font->w % font->cw ==  0);
    ASSERT(font->h / font->ch >= 16);

    uint32_t color_packed = pack_color(color);

    float cw = (float)font->cw;
    float ch = (float)font->ch;

    float newline_offset = 0;
    for (size_t i = 0; i < string.count; i++)
    {
        if (i + 1 < string.count && string.data[i] == '\n')
        {
            newline_offset += ch;
        }
    }

    v2_t at = p;
    at.y += newline_offset;

    for (size_t i = 0; i < string.count; i++)
    {
        char c = string.data[i];

        float cx = (float)(c % 16);
        float cy = (float)(c / 16);

        if (is_newline(c))
        {
            at.y -= ch;
            at.x  = p.x;
        }
        else
        {
            float u0 = cx / 16.0f;
            float u1 = u0 + (1.0f / 16.0f);

            float v0 = cy / 16.0f;
            float v1 = v0 + (1.0f / 16.0f);

            uint32_t i0 = r_immediate_vertex(&(vertex_immediate_t) {
                .pos = { at.x, at.y, 0.0f }, // TODO: Use Z?
                .tex = { u0, v1 },
                .col = color_packed,
            });

            uint32_t i1 = r_immediate_vertex(&(vertex_immediate_t) {
                .pos = { at.x + cw, at.y, 0.0f }, // TODO: Use Z?
                .tex = { u1, v1 },
                .col = color_packed,
            });

            uint32_t i2 = r_immediate_vertex(&(vertex_immediate_t) {
                .pos = { at.x + cw, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = { u1, v0 },
                .col = color_packed,
            });

            uint32_t i3 = r_immediate_vertex(&(vertex_immediate_t) {
                .pos = { at.x, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = { u0, v0 },
                .col = color_packed,
            });

            r_immediate_index(i0);
            r_immediate_index(i1);
            r_immediate_index(i2);
            r_immediate_index(i0);
            r_immediate_index(i2);
            r_immediate_index(i3);

            at.x += cw;
        }
    }

	r_immediate_flush();
}
