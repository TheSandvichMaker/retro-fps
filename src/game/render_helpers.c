// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

void r_immediate_line(r_context_t *rc, v3_t start, v3_t end, v4_t color)
{
    uint32_t color_packed = pack_color(color);

    uint32_t i0 = r_immediate_vertex(rc, (r_vertex_immediate_t){ .pos = start, .col = color_packed });
    r_immediate_index(rc, i0);
    uint32_t i1 = r_immediate_vertex(rc, (r_vertex_immediate_t){ .pos = end,   .col = color_packed });
    r_immediate_index(rc, i1);
}

void r_immediate_line_gradient(r_context_t *rc, v3_t start, v3_t end, v4_t start_color, v4_t end_color)
{
    uint32_t i0 = r_immediate_vertex(rc, (r_vertex_immediate_t){ .pos = start, .col = pack_color(start_color) });
    r_immediate_index(rc, i0);
    uint32_t i1 = r_immediate_vertex(rc, (r_vertex_immediate_t){ .pos = end,   .col = pack_color(end_color) });
    r_immediate_index(rc, i1);
}

void r_immediate_rect2_filled(r_context_t *rc, rect2_t rect, v4_t color)
{
    uint32_t color_packed = pack_color(color);

    uint32_t i0 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.min.x, rect.min.y, 0.0f }, 
        .tex = { 0, 0 },
        .col = color_packed,
    });
    uint32_t i1 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.max.x, rect.min.y, 0.0f }, 
        .tex = { 1, 0 },
        .col = color_packed,
    });
    uint32_t i2 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.max.x, rect.max.y, 0.0f }, 
        .tex = { 1, 1 },
        .col = color_packed,
    });
    uint32_t i3 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.min.x, rect.max.y, 0.0f }, 
        .tex = { 0, 1 },
        .col = color_packed,
    });

    // triangle 1
    r_immediate_index(rc, i0);
    r_immediate_index(rc, i1);
    r_immediate_index(rc, i2);

    // triangle 2
    r_immediate_index(rc, i0);
    r_immediate_index(rc, i2);
    r_immediate_index(rc, i3);
}

void r_immediate_rect2_filled_gradient(r_context_t *rc, rect2_t rect, v4_t colors[4])
{
    uint32_t i0 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.min.x, rect.min.y, 0.0f }, 
        .tex = { 0, 0 },
        .col = pack_color(colors[0]),
    });
    uint32_t i1 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.max.x, rect.min.y, 0.0f }, 
        .tex = { 1, 0 },
        .col = pack_color(colors[1]), 
    });
    uint32_t i2 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.max.x, rect.max.y, 0.0f }, 
        .tex = { 1, 1 },
        .col = pack_color(colors[2]),
    });
    uint32_t i3 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos = { rect.min.x, rect.max.y, 0.0f }, 
        .tex = { 0, 1 },
        .col = pack_color(colors[3]), 
    });

    // triangle 1
    r_immediate_index(rc, i0);
    r_immediate_index(rc, i1);
    r_immediate_index(rc, i2);

    // triangle 2
    r_immediate_index(rc, i0);
    r_immediate_index(rc, i2);
    r_immediate_index(rc, i3);
}

void r_immediate_arrow_gradient(r_context_t *rc, v3_t start, v3_t end, v4_t start_color, v4_t end_color)
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
        r_immediate_line_gradient(rc, v0, v1, end_color, end_color);

        r_immediate_line_gradient(rc, v0, end, end_color, end_color);
    }

    r_immediate_line_gradient(rc, start, shaft_end, start_color, end_color);
}

void r_immediate_arrow(r_context_t *rc, v3_t start, v3_t end, v4_t color)
{
    r_immediate_arrow_gradient(rc, start, end, color, color);
}

void r_immediate_rect3_outline(r_context_t *rc, rect3_t bounds, v4_t color)
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
    r_immediate_line(rc, v000, v100, color);
    r_immediate_line(rc, v100, v110, color);
    r_immediate_line(rc, v110, v010, color);
    r_immediate_line(rc, v010, v000, color);

    // top plane
    r_immediate_line(rc, v001, v101, color);
    r_immediate_line(rc, v101, v111, color);
    r_immediate_line(rc, v111, v011, color);
    r_immediate_line(rc, v011, v001, color);

    // "pillars"
    r_immediate_line(rc, v000, v001, color);
    r_immediate_line(rc, v100, v101, color);
    r_immediate_line(rc, v010, v011, color);
    r_immediate_line(rc, v110, v111, color);
}

void r_immediate_itriangle(r_context_t *rc, uint32_t a, uint32_t b, uint32_t c)
{
    r_immediate_index(rc, a);
    r_immediate_index(rc, b);
    r_immediate_index(rc, c);
}

void r_immediate_iquad(r_context_t *rc, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    r_immediate_index(rc, a);
    r_immediate_index(rc, b);
    r_immediate_index(rc, c);
    r_immediate_index(rc, a);
    r_immediate_index(rc, c);
    r_immediate_index(rc, d);
}

void r_immediate_triangle(r_context_t *rc, triangle_t t, v4_t color)
{
    uint32_t color_packed = pack_color(color);

	v3_t normal = triangle_normal(t.a, t.b, t.c);

    uint32_t i0 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos    = t.a,
        .col    = color_packed,
		.normal = normal,
    });

    uint32_t i1 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos    = t.b,
        .col    = color_packed,
		.normal = normal,
    });

    uint32_t i2 = r_immediate_vertex(rc, (r_vertex_immediate_t){ 
        .pos    = t.c,
        .col    = color_packed,
		.normal = normal,
    });

	r_immediate_index(rc, i0);
	r_immediate_index(rc, i1);
	r_immediate_index(rc, i2);
}

void r_immediate_quad(r_context_t *rc, r_vertex_immediate_t a, r_vertex_immediate_t b, r_vertex_immediate_t c, r_vertex_immediate_t d)
{
    uint32_t i0 = r_immediate_vertex(rc, a);
    uint32_t i1 = r_immediate_vertex(rc, b);
    uint32_t i2 = r_immediate_vertex(rc, c);
    uint32_t i3 = r_immediate_vertex(rc, d);

	r_immediate_index(rc, i0);
	r_immediate_index(rc, i1);
	r_immediate_index(rc, i2);
	r_immediate_index(rc, i0);
	r_immediate_index(rc, i2);
	r_immediate_index(rc, i3);
}

void r_immediate_text(r_context_t *rc, const bitmap_font_t *font, v2_t p, v4_t color, string_t string)
{
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

            uint32_t i0 = r_immediate_vertex(rc, (r_vertex_immediate_t) {
                .pos = { at.x, at.y, 0.0f }, // TODO: Use Z?
                .tex = { u0, v1 },
                .col = color_packed,
            });

            uint32_t i1 = r_immediate_vertex(rc, (r_vertex_immediate_t) {
                .pos = { at.x + cw, at.y, 0.0f }, // TODO: Use Z?
                .tex = { u1, v1 },
                .col = color_packed,
            });

            uint32_t i2 = r_immediate_vertex(rc, (r_vertex_immediate_t) {
                .pos = { at.x + cw, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = { u1, v0 },
                .col = color_packed,
            });

            uint32_t i3 = r_immediate_vertex(rc, (r_vertex_immediate_t) {
                .pos = { at.x, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = { u0, v0 },
                .col = color_packed,
            });

            r_immediate_index(rc, i0);
            r_immediate_index(rc, i1);
            r_immediate_index(rc, i2);
            r_immediate_index(rc, i0);
            r_immediate_index(rc, i2);
            r_immediate_index(rc, i3);

            at.x += cw;
        }
    }
}

void r_immediate_sphere(r_context_t *rc, v3_t p, float r, v4_t color_, size_t slices, size_t stacks)
{
    // source: https://www.danielsieger.com/blog/2021/03/27/generating-spheres.html

    uint32_t color = pack_color(color_);

    m_scoped_temp
    {
        stretchy_buffer(uint32_t) indices = NULL;

        // top vertex
        uint32_t v0 = r_immediate_vertex(rc, (r_vertex_immediate_t) {
            .pos = { p.x, p.y, p.z + r },
            .col = color,
            .normal = { 0, 0, 1 },
        });
        sb_push(indices, v0);

        // vertices per stack/slice
        for (size_t i = 0; i < stacks - 1; i++)
        {
            float phi = PI32*(float)(i + 1) / (float)stacks;
            for (size_t j = 0; j < slices; j++)
            {
                float theta = 2.0f*PI32*(float)j / (float)slices;
                v3_t offset = {
                    .x = sin_ss(phi)*cos_ss(theta),
                    .y = sin_ss(phi)*sin_ss(theta),
                    .z = cos_ss(phi),
                };
                uint32_t index = r_immediate_vertex(rc, (r_vertex_immediate_t) {
                    .pos = add(p, mul(r, offset)),
                    .col = color,
                    .normal = offset,
                });
                sb_push(indices, index);
            }
        }

        // bottom vertex
        uint32_t v1 = r_immediate_vertex(rc, (r_vertex_immediate_t) {
            .pos = { p.x, p.y, p.z - r },
            .col = color,
            .normal = { 0, 0, -1 },
        });
        sb_push(indices, v1);

        // top/bottom triangles
        for (size_t i = 0; i < slices; i++)
        {
            size_t i0 =  i + 1;
            size_t i1 = (i + 1) % slices + 1;
            r_immediate_itriangle(rc, v0, indices[i1], indices[i0]);
            // TODO: FIX THIS. The bottom triangles of the sphere are wrong. Commented out here
            // to avoid them wreaking havoc, but preferably they simply become not-wrong.
#if 0
            i0 =  i               + slices*(stacks - 2) + 1;
            i1 = (i + 1) % slices + slices*(stacks - 2) + 1;
            r_immediate_itriangle(v1, indices[i0], indices[i1]);
#endif
        }

        // add quads
        for (size_t j = 0; j < stacks - 2; j++)
        {
            size_t j0 =  j     *slices + 1;
            size_t j1 = (j + 1)*slices + 1;
            for (size_t i = 0; i < slices; i++)
            {
                size_t i0 = j0 + i;
                size_t i1 = j0 + (i + 1) % slices;
                size_t i2 = j1 + (i + 1) % slices;
                size_t i3 = j1 + i;
                r_immediate_iquad(rc, indices[i0], indices[i1], indices[i2], indices[i3]);
            }
        }
    }
}

void make_quad_vertices(v3_t pos, v2_t size, quat_t rotation, v4_t color,
						r_vertex_immediate_t *a,
						r_vertex_immediate_t *b,
						r_vertex_immediate_t *c,
						r_vertex_immediate_t *d)
{
	v3_t basis_x = quat_rotatev(rotation, v3_from_v2(mul(size, make_v2(0.5, 0)), 0));
	v3_t basis_y = quat_rotatev(rotation, v3_from_v2(mul(size, make_v2(0, 0.5)), 0));

	v3_t pos_a = v3_add3(pos, negate(basis_x), negate(basis_y));
	v3_t pos_b = v3_add3(pos,        basis_x , negate(basis_y));
	v3_t pos_c = v3_add3(pos,        basis_x ,        basis_y );
	v3_t pos_d = v3_add3(pos, negate(basis_x),        basis_y );

	uint32_t color_packed = pack_color(color);

	*a = (r_vertex_immediate_t){
		.pos   = pos_a,
		.tex   = { 0, 0 },
		.col   = color_packed,
	};

	*b = (r_vertex_immediate_t){
		.pos   = pos_b,
		.tex   = { 1, 0 },
		.col   = color_packed,
	};

	*c = (r_vertex_immediate_t){
		.pos   = pos_c,
		.tex   = { 1, 1 },
		.col   = color_packed,
	};

	*d = (r_vertex_immediate_t){
		.pos   = pos_d,
		.tex   = { 0, 1 },
		.col   = color_packed,
	};
}

void r_draw_text(r_context_t *rc, const bitmap_font_t *font, v2_t p, v4_t color, string_t string)
{
    r_immediate_t *imm = r_immediate_begin(rc);
    imm->texture = font->texture;

	r_immediate_text(rc, font, p, color, string);

    r_immediate_end(rc, imm);
}
