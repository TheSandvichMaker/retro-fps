#include "render.h"

static render_api_i inst;
const render_api_i *const render = &inst;

void equip_render_api(render_api_i *api)
{
    inst = *api;
}

uint32_t vertex_format_size[VERTEX_FORMAT_COUNT] = {
    [VERTEX_FORMAT_POS]         = sizeof(v3_t),
    [VERTEX_FORMAT_POS_TEX_COL] = sizeof(vertex_pos_tex_col_t),
    [VERTEX_FORMAT_BRUSH ]      = sizeof(vertex_brush_t),
};

static r_list_t *g_list;

void r_set_command_list(r_list_t *new_list)
{
    g_list = new_list;
}

void r_reset_command_list(void)
{
    g_list->command_list_at = g_list->command_list_base;
    g_list->view_count    = 0;
    g_list->view_stack_at = 0;

    r_view_t view;
    r_default_view(&view);
    r_push_view(&view);
}

static STRING_STORAGE(64) g_next_command_identifier;

void r_command_identifier(string_t identifier)
{
    STRING_INTO_STORAGE(g_next_command_identifier, identifier);
}

void r_submit_command(void *command)
{
    ASSERT(g_list);

    r_command_base_t *base = command;

    if (ALWAYS(g_list->view_stack_at > 0))
    {
        base->view = g_list->view_stack[g_list->view_stack_at-1];
    }
    else
    {
        base->view = 0;
    }

#ifndef NDEBUG
    if (g_next_command_identifier.count > 0)
    {
        base->identifier = string_copy(temp, STRING_FROM_STORAGE(g_next_command_identifier));
    }
    g_next_command_identifier.count = 0;
#endif

    if (ALWAYS(base->kind > R_COMMAND_NONE && base->kind < R_COMMAND_COUNT))
    {
        size_t command_size = 0;

        switch (base->kind)
        {
            case R_COMMAND_MODEL:     command_size = sizeof(r_command_model_t); break;
            case R_COMMAND_IMMEDIATE: command_size = sizeof(r_command_immediate_t); break;

            INVALID_DEFAULT_CASE;
        }

        size_t size_used = align_forward(g_list->command_list_at - g_list->command_list_base, RENDER_COMMAND_ALIGN);
        size_t size_left = g_list->command_list_size - size_used;
        if (ALWAYS(size_left >= command_size))
        {
            char *at = align_address(g_list->command_list_at, RENDER_COMMAND_ALIGN);
            copy_memory(at, command, command_size);

            g_list->command_list_at = at + command_size;
        }
    }
}

void r_push_view(const r_view_t *view)
{
    if (ALWAYS(g_list->view_count < R_MAX_VIEWS))
    {
        r_view_index_t index = g_list->view_count++;
        copy_struct(&g_list->views[index], view);

        if (ALWAYS(g_list->view_stack_at < R_MAX_VIEWS))
        {
            g_list->view_stack[g_list->view_stack_at++] = index;
        }
    }
}

void r_push_view_screenspace(void)
{
    r_view_t view = { 0 };

    int w, h;
    render->get_resolution(&w, &h);

    view.clip_rect = (rect2_t) {
        .x0 = 0.0f,
        .y0 = 0.0f,
        .x1 = (float)w,
        .y1 = (float)h,
    };

    float a = 2.0f / (float)w;
    float b = 2.0f / (float)h;

    view.camera = make_m4x4(
        a, 0, 0, -1,
        0, b, 0, -1,
        0, 0, 1,  0,
        0, 0, 0,  1
    );
    view.projection = m4x4_identity;

    r_push_view(&view);
}

void r_default_view(r_view_t *view)
{
    zero_struct(view);

    int w, h;
    render->get_resolution(&w, &h);

    view->clip_rect = (rect2_t) {
        .x0 = 0.0f,
        .y0 = 0.0f,
        .x1 = (float)w,
        .y1 = (float)h,
    };

    view->camera     = m4x4_identity;
    view->projection = m4x4_identity;
}

void r_get_view(r_view_t *view)
{
    if (g_list->view_stack_at > 0)
    {
        *view = g_list->views[g_list->view_stack[g_list->view_stack_at-1]];
    }
    else
    {
        r_default_view(view);
    }
}

void r_pop_view(void)
{
    if (ALWAYS(g_list->view_stack_at > 1))
    {
        g_list->view_stack_at--;
    }
}

static bool r_immediate_submit_vertices(void);

typedef struct r_immediate_state_s
{
    m4x4_t                 transform;
    bool                   no_depth_test;

    resource_handle_t      texture;
    r_primitive_topology_t topology;

    uint32_t               icount;
    uint16_t               ibuffer[MAX_IMMEDIATE_INDICES];

    uint32_t               vcount;
    vertex_pos_tex_col_t   vbuffer[MAX_IMMEDIATE_VERTICES];
} r_immediate_state_t;

static r_immediate_state_t g_immediate_state = {
    .transform = M4X4_IDENTITY_INIT,
};

void r_immediate_topology(r_primitive_topology_t topology)
{
    r_immediate_flush(); // flush if necessary
    g_immediate_state.topology = topology;
}

void r_immediate_texture(resource_handle_t texture)
{
    r_immediate_flush(); // flush if necessary
    g_immediate_state.texture = texture;
}

void r_immediate_depth_test(bool enabled)
{
    g_immediate_state.no_depth_test = !enabled;
}

void r_immediate_transform(const m4x4_t *transform)
{
    g_immediate_state.transform = *transform;
}

uint16_t r_immediate_vertex(const vertex_pos_tex_col_t *vertex)
{
    uint16_t result = UINT16_MAX;

    if (g_immediate_state.vcount >= MAX_IMMEDIATE_VERTICES)
    {
        r_immediate_submit_vertices();
    }

    result = (uint16_t)g_immediate_state.vcount++;
    g_immediate_state.vbuffer[result] = *vertex;

    return result;
}

void r_immediate_index(uint16_t index)
{
    if (g_immediate_state.icount >= MAX_IMMEDIATE_INDICES)
    {
        r_immediate_submit_vertices();
    }

    g_immediate_state.ibuffer[g_immediate_state.icount++] = index;
}

void r_immediate_line(v3_t start, v3_t end, v3_t color)
{
    uint16_t i0 = r_immediate_vertex(&(vertex_pos_tex_col_t){ .pos = start, .col = color });
    r_immediate_index(i0);
    uint16_t i1 = r_immediate_vertex(&(vertex_pos_tex_col_t){ .pos = end,   .col = color });
    r_immediate_index(i1);
}

static bool r_immediate_submit_vertices(void)
{
    if (g_immediate_state.vcount > 0)
    {
        r_submit_command(&(r_command_immediate_t){
            .base = {
                .kind = R_COMMAND_IMMEDIATE,
            },

            .transform     = g_immediate_state.transform,
            .no_depth_test = g_immediate_state.no_depth_test,

            .texture       = g_immediate_state.texture,
            .topology      = g_immediate_state.topology,

            .icount        = g_immediate_state.icount,
            .ibuffer       = m_copy_array(temp, g_immediate_state.ibuffer, g_immediate_state.icount),

            .vcount        = g_immediate_state.vcount,
            .vbuffer       = m_copy_array(temp, g_immediate_state.vbuffer, g_immediate_state.vcount),
        });

        g_immediate_state.vcount = 0;
        g_immediate_state.icount = 0;

        return true;
    }

    return false;
}

void r_immediate_flush(void)
{
    if (r_immediate_submit_vertices())
    {
        g_immediate_state.transform     = m4x4_identity;
        g_immediate_state.no_depth_test = false;
        g_immediate_state.texture       = (resource_handle_t){0};
        g_immediate_state.topology      = R_PRIMITIVE_TOPOLOGY_TRIANGELIST;
    }
}

void r_immediate_text(const bitmap_font_t *font, v2_t p, v3_t color, string_t string)
{
    r_push_view_screenspace();
    r_immediate_texture(font->texture);
    r_immediate_topology(R_PRIMITIVE_TOPOLOGY_TRIANGELIST);
    r_immediate_depth_test(false);

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

            uint16_t i0 = r_immediate_vertex(&(vertex_pos_tex_col_t) {
                .pos = (v3_t) { at.x, at.y, 0.0f }, // TODO: Use Z?
                .tex = (v2_t) { u0, v1 },
                .col = color,
            });

            uint16_t i1 = r_immediate_vertex(&(vertex_pos_tex_col_t) {
                .pos = (v3_t) { at.x + cw, at.y, 0.0f }, // TODO: Use Z?
                .tex = (v2_t) { u1, v1 },
                .col = color,
            });

            uint16_t i2 = r_immediate_vertex(&(vertex_pos_tex_col_t) {
                .pos = (v3_t) { at.x + cw, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = (v2_t) { u1, v0 },
                .col = color,
            });

            uint16_t i3 = r_immediate_vertex(&(vertex_pos_tex_col_t) {
                .pos = (v3_t) { at.x, at.y + ch, 0.0f }, // TODO: Use Z?
                .tex = (v2_t) { u0, v0 },
                .col = color,
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

    r_command_identifier(string_format(temp, "text: %.*s", strexpand(string)));
    r_immediate_flush();

    r_pop_view();
}