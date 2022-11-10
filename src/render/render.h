#ifndef RENDER_H
#define RENDER_H

#include "core/core.h"

#define RENDER_COMMAND_ALIGN 16

#define MAX_IMMEDIATE_INDICES  (16384)
#define MAX_IMMEDIATE_VERTICES (8192)

typedef struct bitmap_font_t
{
    unsigned w, h, cw, ch;
    resource_handle_t texture;
} bitmap_font_t;

extern v3_t g_debug_colors[6];

#define COLOR32_WHITE   pack_rgba(1, 1, 1, 1)
#define COLOR32_RED     pack_rgba(1, 0, 0, 1)
#define COLOR32_GREEN   pack_rgba(0, 1, 0, 1)
#define COLOR32_BLUE    pack_rgba(0, 0, 1, 1)
#define COLOR32_MAGENTA pack_rgba(1, 0, 1, 1)
#define COLOR32_CYAN    pack_rgba(0, 1, 1, 1)
#define COLOR32_YELLOW  pack_rgba(1, 1, 0, 1)

static inline v3_t r_debug_color(size_t i)
{
    return g_debug_colors[i % ARRAY_COUNT(g_debug_colors)];
}

typedef enum pixel_format_e
{
    PIXEL_FORMAT_R8,
    PIXEL_FORMAT_RG8,
    PIXEL_FORMAT_RGBA8,
    PIXEL_FORMAT_SRGB8_A8,
} pixel_format_t;

typedef enum texture_flags_e
{
    TEXTURE_FLAG_CUBEMAP = 0x1,
} upload_texture_flags_e;

typedef struct upload_texture_s
{
    pixel_format_t format;
    uint32_t w, h, pitch;
    uint32_t flags;
    union
    {
        void *pixels;
        void *faces[6];
    };
} upload_texture_t;

typedef enum vertex_format_e
{
    VERTEX_FORMAT_POS,
    VERTEX_FORMAT_IMMEDIATE,
    VERTEX_FORMAT_BRUSH,
    VERTEX_FORMAT_COUNT,
} vertex_format_t;

typedef enum r_primitive_topology_e
{
    R_PRIMITIVE_TOPOLOGY_TRIANGELIST, // default
    R_PRIMITIVE_TOPOLOGY_TRIANGESTRIP,
    R_PRIMITIVE_TOPOLOGY_LINELIST,
    R_PRIMITIVE_TOPOLOGY_LINESTRIP,
    R_PRIMITIVE_TOPOLOGY_POINTLIST,
} r_primitive_topology_t;

extern uint32_t vertex_format_size[VERTEX_FORMAT_COUNT];

typedef struct vertex_pos_t
{
    v3_t pos;
} vertex_pos_t;

typedef struct vertex_immediate_t
{
    v3_t pos;
    v2_t tex;
    uint32_t col;
} vertex_immediate_t;

typedef struct vertex_brush_t
{
    v3_t pos;
    v2_t tex;
    v2_t tex_lightmap;
} vertex_brush_t;

typedef struct upload_model_s
{
    r_primitive_topology_t topology;

    vertex_format_t vertex_format;
    uint32_t vertex_count;
    const void *vertices;

    uint32_t index_count;
    const uint16_t *indices;
} upload_model_t;

enum { MISSING_TEXTURE_SIZE = 64 };

typedef struct render_api_i
{
    void (*get_resolution)(int *w, int *h);

    resource_handle_t (*upload_texture)(const upload_texture_t *params);
    void (*destroy_texture)(resource_handle_t texture);

    resource_handle_t (*upload_model)(const upload_model_t *params);
    void (*destroy_model)(resource_handle_t model);
} render_api_i;

void equip_render_api(render_api_i *api);
extern const render_api_i *const render;

typedef struct r_view_s
{
    rect2_t clip_rect;
    m4x4_t camera, projection;
    resource_handle_t skybox;
} r_view_t;

typedef enum r_command_kind_e
{
    R_COMMAND_NONE, // is here just to catch mistake of not initializing the kind 

    R_COMMAND_MODEL,
    R_COMMAND_IMMEDIATE,
    
    R_COMMAND_COUNT,
} r_command_kind_t;

typedef struct r_command_base_s
{
    unsigned char kind;
    unsigned char view;
#ifndef NDEBUG
    string_t identifier;
#endif
} r_command_base_t;

void r_command_identifier(string_t identifier);

typedef struct r_command_model_s
{
    r_command_base_t base;

    m4x4_t transform;

    resource_handle_t model;
    resource_handle_t texture;
    resource_handle_t lightmap;
} r_command_model_t;

typedef struct r_command_immediate_t
{
    r_command_base_t base;

    m4x4_t                 transform;
    bool                   no_depth_test;

    resource_handle_t      texture;
    r_primitive_topology_t topology;

    float                  depth_bias;

    uint32_t               icount;
    uint16_t              *ibuffer;

    uint32_t               vcount;
    vertex_immediate_t    *vbuffer;
} r_command_immediate_t;

// immediate mode API
void     r_immediate_topology(r_primitive_topology_t topology);
void     r_immediate_texture(resource_handle_t texture);
void     r_immediate_depth_test(bool enabled);
void     r_immediate_depth_bias(float bias);
void     r_immediate_transform(const m4x4_t *transform);
uint16_t r_immediate_vertex(const vertex_immediate_t *vertex);
void     r_immediate_index(uint16_t index);
void     r_immediate_flush(void);

enum { R_MAX_VIEWS = 32 };
typedef unsigned char r_view_index_t;

typedef struct r_list_s
{
    r_view_index_t view_count;
    r_view_t views[R_MAX_VIEWS];

    r_view_index_t view_stack_at;
    r_view_index_t view_stack[R_MAX_VIEWS];

    size_t command_list_size;
    char *command_list_base;
    char *command_list_at;
} r_list_t;

void r_set_command_list(r_list_t *list);
void r_reset_command_list(void);

void r_submit_command(void *command);

void r_default_view(r_view_t *view);
void r_push_view(const r_view_t *view);
void r_push_view_screenspace(void);
void r_copy_top_view(r_view_t *view);
r_view_t *r_get_top_view(void);
v3_t r_to_view_space(const r_view_t *view, v3_t p, float w);
void r_pop_view(void);

static inline v4_t linear_to_srgb(v4_t color)
{
    // TODO: more accurate srgb transforms?
    color.xyz = (v3_t){
        .x = sqrt_ss(color.x),
        .y = sqrt_ss(color.y),
        .z = sqrt_ss(color.z),
    };
    return color;
}

static inline v4_t srgb_to_linear(v4_t color)
{
    // TODO: more accurate srgb transforms?
    color.xyz = (v3_t){
        .x = color.x*color.x,
        .y = color.y*color.y,
        .z = color.z*color.z,
    };
    return color;
}

static inline uint32_t pack_color(v4_t color)
{
    color.x = CLAMP(color.x, 0.0f, 1.0f);
    color.y = CLAMP(color.y, 0.0f, 1.0f);
    color.z = CLAMP(color.z, 0.0f, 1.0f);
    color.w = CLAMP(color.w, 0.0f, 1.0f);

    uint32_t result = (((uint32_t)(255.0f*color.x) <<  0) |
                       ((uint32_t)(255.0f*color.y) <<  8) |
                       ((uint32_t)(255.0f*color.z) << 16) |
                       ((uint32_t)(255.0f*color.w) << 24));
    return result;
}

static inline uint32_t pack_rgba(float r, float g, float b, float a)
{
    return pack_color((v4_t){r, g, b, a});
}

static inline uint32_t pack_rgb(float r, float g, float b)
{
    return pack_color((v4_t){r, g, b, 1.0f});
}

static inline v4_t unpack_color(uint32_t color)
{
    float rcp_255 = 1.0f / 255.0f;

    v4_t result;
    result.x = rcp_255*(float)((color >>  0) & 0xFF);
    result.y = rcp_255*(float)((color >>  8) & 0xFF);
    result.z = rcp_255*(float)((color >> 16) & 0xFF);
    result.w = rcp_255*(float)((color >> 24) & 0xFF);
    return result;
}

#define R_DRAWCALL_SCREENSPACE \
    for (int i__ = (r_immediate_depth_test(false), r_push_view_screenspace(), 1); \
         i__; \
         i__ = (r_immediate_flush(), r_pop_view(), 0))

#endif /* RENDER_H */
