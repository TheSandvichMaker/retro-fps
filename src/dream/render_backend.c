#include "render_backend.h"

static render_api_i inst;
const render_api_i *const render = &inst;

void equip_render_api(render_api_i *api)
{
    inst = *api;
}

uint32_t vertex_format_size[R_VERTEX_FORMAT_COUNT] = {
    [R_VERTEX_FORMAT_POS]       = sizeof(r_vertex_pos_t),
    [R_VERTEX_FORMAT_IMMEDIATE] = sizeof(r_vertex_immediate_t),
    [R_VERTEX_FORMAT_BRUSH]     = sizeof(r_vertex_brush_t),
};
