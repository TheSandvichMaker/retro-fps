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
    m_reset_and_decommit(&rc->commands->debug_render_data_arena);

    rc->commands->views_count        = 0;
    rc->commands->commands_count     = 0;
    rc->commands->data_at            = 0;
    rc->commands->imm_indices_count  = 0;
    rc->commands->imm_vertices_count = 0;
    rc->commands->ui_rects_count     = 0;

    rc->screen_layers_stack.at = 0;
    stack_push(rc->screen_layers_stack, R_SCREEN_LAYER_SCENE);

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

        view.view_matrix = make_m4x4(
            a, 0, 0, -1,
            0, b, 0, -1,
            0, 0, 1,  0,
            0, 0, 0,  1
        );
        view.proj_matrix = M4X4_IDENTITY;

        rc->screenspace = r_make_view(rc, &view);
    }
}

void r_push_command_identifier(r_context_t *rc, string_t info)
{
    (void)rc; (void)info;

#if DREAM_SLOW
    arena_t *debug_arena = &rc->commands->debug_render_data_arena;
    slist_appends(&rc->identifier_stack, debug_arena, info);
    rc->next_command_identifier = slist_flatten_with_separator(&rc->identifier_stack, debug_arena, S("\n"), 0);
#endif
}

void r_pop_command_identifier(r_context_t *rc)
{
    (void)rc;

#if DREAM_SLOW
    DEBUG_ASSERT(!slist_empty(&rc->identifier_stack));
    slist_pop_last(&rc->identifier_stack);
    arena_t *debug_arena = &rc->commands->debug_render_data_arena;
    rc->next_command_identifier = slist_flatten_with_separator(&rc->identifier_stack, debug_arena, S("\n"), 0);
#endif

}

DREAM_INLINE r_command_key_t r_command_key(r_context_t *rc, r_command_kind_t kind, float depth, uint32_t material_id)
{
    r_command_key_t result = {
        .screen_layer = stack_top(rc->screen_layers_stack),
        .view         = stack_top(rc->views_stack),
        .view_layer   = stack_top(rc->view_layers_stack),
        .kind         = kind,
        .depth        = r_encode_command_depth(depth),
        .material_id  = material_id,
    };
    return result;
}

void r_push_command(r_context_t *rc, r_command_t command)
{
    ASSERT(command.key.view < rc->commands->views_count);

    r_command_buffer_t *commands = rc->commands;
#if DREAM_SLOW
    if (!command.identifier.count)
    {
        command.identifier = rc->next_command_identifier;
    }
#endif

    if (ALWAYS(commands->commands_count < commands->commands_capacity))
    {
        commands->commands[commands->commands_count++] = command;
    }
}

void *r_allocate_command_data(r_context_t *rc, size_t size)
{
    void *result = NULL;

    if (size > 0)
    {
        size_t at_aligned = align_forward(rc->commands->data_at, R_RENDER_COMMAND_ALIGN);
        size_t size_left  = rc->commands->data_capacity - at_aligned;

        if (ALWAYS(size <= size_left))
        {
            result = &rc->commands->data[at_aligned];
            rc->commands->data_at = (uint32_t)(at_aligned + size);
        }
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

    pw = mul(view->view_matrix, pw);
    pw = mul(view->proj_matrix, pw);
    pw.xyz = div(pw.xyz, pw.w);

    int width, height;
    render->get_resolution(&width, &height);

    pw.x += 1.0f;
    pw.y += 1.0f;
    pw.x *= 0.5f*(float)width;
    pw.y *= 0.5f*(float)height;

    return pw.xyz;
}

void r_push_view_layer(r_context_t *rc, r_view_layer_t layer)
{
    if (ALWAYS(stack_nonfull(rc->view_layers_stack)))
    {
        stack_push(rc->view_layers_stack, layer);
    }
}

void r_pop_view_layer(r_context_t *rc)
{
    if (ALWAYS(stack_nonempty(rc->view_layers_stack)))
    {
        stack_pop(rc->view_layers_stack);
    }
}

void r_push_layer(r_context_t *rc, r_screen_layer_t layer)
{
    if (ALWAYS(stack_nonfull(rc->screen_layers_stack)))
    {
        stack_push(rc->screen_layers_stack, layer);
    }
}

void r_pop_layer(r_context_t *rc)
{
    if (ALWAYS(stack_nonempty(rc->screen_layers_stack)))
    {
        stack_pop(rc->screen_layers_stack);
    }
}

void r_draw_mesh(r_context_t *rc, m4x4_t transform, mesh_handle_t mesh, const r_material_t *material)
{
    r_command_t command = {
        .key  = r_command_key(rc, R_COMMAND_MODEL, 0.0, 0),
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

    rc->current_immediate = imm;

    return imm;
}

void r_immediate_end(r_context_t *rc, r_immediate_t *imm)
{
    ASSERT(rc->current_immediate == imm);

    r_command_t command = {
        .key  = r_command_key(rc, R_COMMAND_IMMEDIATE, 0.0, imm->shader),
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

void r_ui_texture(r_context_t *rc, texture_handle_t texture)
{
    if (!RESOURCE_HANDLES_EQUAL(rc->current_ui_texture, texture))
    {
        r_flush_ui_rects(rc);
    }

    rc->current_ui_texture = texture;
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
    if (rc->current_ui_rect_count > 0)
    {
        r_command_ui_rects_t *data = r_allocate_command_data(rc, sizeof(r_command_ui_rects_t));
        data->texture = rc->current_ui_texture;
        data->first = rc->current_first_ui_rect;
        data->count = rc->current_ui_rect_count;

        r_push_command(rc, (r_command_t){
            .key  = r_command_key(rc, R_COMMAND_UI_RECTS, 0.0, 0),
            .data = data,
        });
    }

#if 0
    OK THE PROBLEM NOW IS that it seems first of all that a view is being resolved
    without having anything rendered to it, and then it's being rendered to afterwards
    and not resolved. The second resolve that lets me actually see it only gets triggered
    if I flush UI rects. Adding that check for count above, if there are no UI rects now
    the image ends up blank.

    OK besides that here's an issue: rc->screenspace is still being used. It shouldn't be.
    UI for the game should be in the UI layer of the scene view, not a separate screenspace
    view. So that needs to be solved
#endif

    rc->current_first_ui_rect = rc->commands->ui_rects_count;
    rc->current_ui_rect_count = 0;
    rc->current_ui_texture    = NULL_HANDLE(texture_handle_t);
}
