#include "render.h"

#define SCREENSPACE_VIEW_INDEX (1)

v3_t g_debug_colors[] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 },
    { 1, 1, 0 },
    { 1, 0, 1 },
    { 0, 1, 1 },
};

static render_api_i inst;
const render_api_i *const render = &inst;

void equip_render_api(render_api_i *api)
{
    inst = *api;
}

uint32_t vertex_format_size[VERTEX_FORMAT_COUNT] = {
    [VERTEX_FORMAT_POS]       = sizeof(vertex_pos_t),
    [VERTEX_FORMAT_IMMEDIATE] = sizeof(vertex_immediate_t),
    [VERTEX_FORMAT_BRUSH]     = sizeof(vertex_brush_t),
};

static r_list_t *g_list;

void r_set_command_list(r_list_t *new_list)
{
    g_list = new_list;
    r_reset_command_list();
}

void r_reset_command_list(void)
{
    g_list->command_list_at = g_list->command_list_base;
    g_list->view_count    = 0;
    g_list->view_stack_at = 0;

    r_view_t default_view;
    r_default_view(&default_view);
    r_push_view(&default_view);

    // TODO: This view situation is a disaster
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
    if (ALWAYS(g_list->view_count < R_MAX_VIEWS))
    {
        r_view_index_t index = SCREENSPACE_VIEW_INDEX;

        if (ALWAYS(g_list->view_stack_at < R_MAX_VIEWS))
        {
            g_list->view_stack[g_list->view_stack_at++] = index;
        }
    }
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

static thread_local r_view_t g_null_view; // should never be necessary

r_view_t *r_get_top_view(void)
{
    r_view_t *result = NULL; 

    if (ALWAYS(g_list->view_stack_at > 0))
    {
        result = &g_list->views[g_list->view_stack[g_list->view_stack_at-1]];
    }
    else
    {
        result = &g_null_view;
        r_default_view(result);
    }

    return result;
}

void r_copy_top_view(r_view_t *view)
{
    if (ALWAYS(g_list->view_stack_at > 0))
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

    float                  depth_bias;

    uint32_t               icount;
    uint16_t               ibuffer[MAX_IMMEDIATE_INDICES];

    uint32_t               vcount;
    vertex_immediate_t   vbuffer[MAX_IMMEDIATE_VERTICES];
} r_immediate_state_t;

static r_immediate_state_t g_immediate_state = {
    .transform = M4X4_IDENTITY_INIT,
};

//
// this immediate mode rendering API makes me so sad
// I will now take a break and stop streaming for a while
//
// I will come back and make the API unsad.
//

void r_immediate_topology(r_primitive_topology_t topology)
{
    if (g_immediate_state.icount > 0)
        r_immediate_flush(); // flush if necessary

    g_immediate_state.topology = topology;
}

void r_immediate_texture(resource_handle_t texture)
{
    if (g_immediate_state.icount > 0)
        r_immediate_flush(); // flush if necessary

    g_immediate_state.texture = texture;
}

void r_immediate_depth_test(bool enabled)
{
    g_immediate_state.no_depth_test = !enabled;
}

void r_immediate_depth_bias(float bias)
{
    g_immediate_state.depth_bias = bias;
}

void r_immediate_transform(const m4x4_t *transform)
{
    g_immediate_state.transform = *transform;
}

uint16_t r_immediate_vertex(const vertex_immediate_t *vertex)
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
            .depth_bias    = g_immediate_state.depth_bias,

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
    r_immediate_submit_vertices();
    g_immediate_state.transform     = m4x4_identity;
    g_immediate_state.no_depth_test = false;
    g_immediate_state.texture       = (resource_handle_t){0};
    g_immediate_state.topology      = R_PRIMITIVE_TOPOLOGY_TRIANGELIST;
    g_immediate_state.depth_bias    = 0.0f;
}

v3_t r_to_view_space(const r_view_t *view, v3_t p, float w)
{
    v4_t pw = { p.x, p.y, p.z, w };

    pw = mul(view->camera, pw);
    pw = mul(view->projection, pw);
    pw.xyz = div(pw.xyz, pw.w);

    int width, height;
    render->get_resolution(&width, &height);

    pw.x += 1.0f;
    pw.y += 1.0f;
    pw.x *= 0.5f*(float)width;
    pw.y *= 0.5f*(float)height;

    return pw.xyz;
}
