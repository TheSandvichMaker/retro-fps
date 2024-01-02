#include "render_backend.h"
#include "core/math.h"

static render_api_i inst;
const render_api_i *const render = &inst;

void equip_render_api(render_api_i *api)
{
    inst = *api;
}

uint32_t r_vertex_format_size[R_VERTEX_FORMAT_COUNT] = {
    [R_VERTEX_FORMAT_POS]       = sizeof(r_vertex_pos_t),
    [R_VERTEX_FORMAT_IMMEDIATE] = sizeof(r_vertex_immediate_t),
    [R_VERTEX_FORMAT_BRUSH]     = sizeof(r_vertex_brush_t),
};

DREAM_LOCAL uint64_t r_encode_command_depth(float depth)
{
    float    depth_normalized = saturate(depth / R_COMMAND_DEPTH_FAR_PLANE);
    uint64_t result           = (F32_AS_U32(depth_normalized) >> 3) & MASK_BITS(20);
    return result;
}
