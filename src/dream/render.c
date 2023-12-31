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

DREAM_INLINE void r_init_immediate(r_context_t *rc, r_immediate_t *imm)
{
	imm->shader     = R_SHADER_FLAT;
	imm->topology   = R_TOPOLOGY_TRIANGLELIST;
	imm->blend_mode = R_BLEND_PREMUL_ALPHA;
	imm->cull_mode  = R_CULL_NONE;
	imm->clip_rect  = (rect2_t){ 0, 0, 0, 0 };
	imm->texture    = NULL_TEXTURE_HANDLE;
    imm->transform  = M4X4_IDENTITY;
	imm->use_depth  = false;
	imm->depth_bias = 0.0f;

	imm->icount  = 0;
	imm->vcount  = 0;
	imm->ioffset = rc->commands->imm_indices_count;
	imm->voffset = rc->commands->imm_vertices_count;
}

void r_init_render_context(r_context_t *rc, r_command_buffer_t *commands)
{
    zero_struct(rc);

    rc->commands = commands;
    r_init_immediate(rc, rc->current_immediate);

    rc->commands->views_count        = 0;
    rc->commands->commands_count     = 0;
    rc->commands->data_at            = 0;
    rc->commands->imm_indices_count  = 0;
    rc->commands->imm_vertices_count = 0;
    rc->commands->ui_rects_count     = 0;

    rc->layers_stack.at = 0;
    stack_push(rc->layers_stack, R_SCREEN_LAYER_SCENE);

    rc->views_stack.at = 0;

    // make screenspace view
    {
        r_view_t view = {0};

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
        view.projection = M4X4_IDENTITY;

        rc->screenspace = r_make_view(rc, &view);
    }
}

void r_command_identifier(r_context_t *rc, string_t identifier)
{
    // TODO: reimplement
    (void)rc;
    (void)identifier;
}

void r_push_command(r_context_t *rc, r_command_t command)
{
    ASSERT(command.key.view < rc->views_stack.at);

    r_command_buffer_t *commands = rc->commands;

    if (ALWAYS(commands->commands_count < commands->commands_capacity))
    {
        commands->commands[commands->commands_count++] = command;
    }
}

void *r_allocate_command_data(r_context_t *rc, size_t size)
{
    void *result = NULL;

    size_t at_aligned = align_forward(rc->commands->data_at, R_RENDER_COMMAND_ALIGN);
    size_t size_left  = rc->commands->data_capacity - at_aligned;

    if (ALWAYS(size_left <= size))
    {
        result = &rc->commands->data[at_aligned];
        rc->commands->data_at = (uint32_t)(at_aligned + size);
    }

    return result;
}

r_view_index_t r_make_view(r_context_t *rc, const r_view_t *view)
{
    r_view_index_t result = 0;

    if (ALWAYS(rc->commands->views_count < rc->commands->views_capacity))
    {
        result = rc->commands->views_count++;

        r_view_t *result_view = &rc->commands->views[result];
        copy_struct(result_view, view);
    }

    return result;
}

void r_push_view(r_context_t *rc, r_view_index_t index)
{
    if (ALWAYS(stack_nonfull(rc->views_stack)))
    {
        stack_push(rc->views_stack, index);
    }
}

void r_pop_view(r_context_t *rc)
{
    if (ALWAYS(stack_nonempty(rc->views_stack)))
    {
        stack_pop(rc->views_stack);
    }
}

r_view_t *r_get_view(r_context_t *rc)
{
    r_view_t *result = NULL; 

    if (ALWAYS(stack_nonempty(rc->views_stack)))
    {
        r_view_index_t index = stack_top(rc->views_stack);
        result = &rc->commands->views[index];
    }
    else
    {
        // who knows what you will find, funny man...
        static r_view_t null_view = {0};
        result = &null_view;
    }

    return result;
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

void r_push_layer(r_context_t *rc, r_screen_layer_t layer)
{
    if (ALWAYS(stack_nonfull(rc->layers_stack)))
    {
        stack_push(rc->layers_stack, layer);
    }
}

void r_pop_layer(r_context_t *rc)
{
    if (ALWAYS(stack_nonempty(rc->layers_stack)))
    {
        stack_pop(rc->layers_stack);
    }
}

void r_draw_mesh(r_context_t *rc, m4x4_t transform, mesh_handle_t mesh, const r_material_t *material)
{
    r_command_t command = {
        .key = {
            .screen_layer = R_SCREEN_LAYER_SCENE,
            .view         = stack_top(rc->views_stack),
            .cmd          = false,
            .depth        = 0, // TODO
            .material_id  = 0, // TODO
        },
        .data = r_allocate_command_data(rc, sizeof(r_command_model_t)),
    };

    r_command_model_t *data = command.data;
    data->transform = transform;
    data->mesh      = mesh;
    data->material  = material;

    r_push_command(rc, command);
}

r_immediate_t *r_immediate_begin(r_context_t *rc)
{
    ASSERT(rc->current_immediate == NULL);

    r_immediate_t *imm = r_allocate_command_data(rc, sizeof(r_immediate_t));
    r_init_immediate(rc, imm);

    return imm;
}

void r_immediate_end(r_context_t *rc, r_immediate_t *imm)
{
    ASSERT(rc->current_immediate == imm);

    r_command_t command = {
        .key = {
            .screen_layer = stack_top(rc->layers_stack),
            .view         = stack_top(rc->views_stack),
            .cmd          = true,
            .command = {
                .kind = R_COMMAND_IMMEDIATE,
            },
        },
        .data = imm,
    };

    r_push_command(rc, command);

    rc->current_immediate = NULL;
}

uint32_t r_immediate_vertex(r_context_t *rc, r_vertex_immediate_t vertex)
{
    uint32_t result = UINT16_MAX;

    if (ALWAYS(rc->commands->imm_vertices_count < rc->commands->imm_vertices_capacity))
    {
        if (ALWAYS(rc->current_immediate))
        {
            rc->current_immediate->vcount += 1;
        }

        result = rc->commands->imm_vertices_count++;
        rc->commands->imm_vertices[result] = vertex;
    }

    return result;
}

void r_immediate_index(r_context_t *rc, uint32_t index)
{
    if (ALWAYS(rc->commands->imm_indices_count < rc->commands->imm_indices_capacity))
    {
        if (ALWAYS(rc->current_immediate))
        {
            rc->current_immediate->icount += 1;
        }

        rc->commands->imm_indices[rc->commands->imm_indices_count++] = index;
    }
}

void r_ui_rect(r_context_t *rc, r_ui_rect_t rect)
{
    // TODO: But when do we even submit the command? dfbkdmgbdgr
	if (ALWAYS(rc->commands->ui_rects_count < rc->commands->ui_rects_capacity))
	{
		v2_t  dim   = rect2_dim(rect.rect);
		float limit = 0.5f*min(dim.x, dim.y);

		rect.roundedness.x = CLAMP(rect.roundedness.x, 0.0f, limit);
		rect.roundedness.z = CLAMP(rect.roundedness.z, 0.0f, limit);
		rect.roundedness.y = CLAMP(rect.roundedness.y, 0.0f, limit);
		rect.roundedness.w = CLAMP(rect.roundedness.w, 0.0f, limit);

        rc->commands->ui_rects[rc->commands->ui_rects_count++] = rect;
        rc->current_ui_rect_count += 1;
	}
}

void r_flush_ui_rects(r_context_t *rc)
{
    r_push_command(rc, (r_command_t){
        .key = {
            .screen_layer = stack_top(rc->layers_stack),
            .view         = stack_top(rc->views_stack),
            .cmd = true,
            .command = {
                .kind = R_COMMAND_UI_RECTS,
                .ui_rects = {
                    .first = rc->current_first_ui_rect,
                    .count = rc->current_ui_rect_count,
                },
            },
        },
    });

    rc->current_first_ui_rect = rc->commands->ui_rects_count;
    rc->current_ui_rect_count = 0;
}
