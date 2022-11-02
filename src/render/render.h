#ifndef RENDER_H
#define RENDER_H

#include "core/core.h"

#define RENDER_COMMAND_ALIGN 16

#define MAX_IMMEDIATE_INDICES  (16384)
#define MAX_IMMEDIATE_VERTICES (8192)

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
    VERTEX_FORMAT_POS_TEX_COL,
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

typedef struct vertex_pos_tex_col_s
{
    v3_t pos;
    v2_t tex;
    v3_t col;
} vertex_pos_tex_col_t;

typedef struct vertex_brush_t
{
    v3_t pos;
    v2_t tex;
    v2_t tex_lightmap;
    v3_t col;
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

    uint32_t               icount;
    uint16_t              *ibuffer;

    uint32_t               vcount;
    vertex_pos_tex_col_t  *vbuffer;
} r_command_immediate_t;

// immediate mode API

void     r_immediate_topology(r_primitive_topology_t topology);
void     r_immediate_texture(resource_handle_t texture);
void     r_immediate_depth_test(bool enabled);
void     r_immediate_transform(const m4x4_t *transform);
uint16_t r_immediate_vertex(const vertex_pos_tex_col_t *vertex);
void     r_immediate_index(uint16_t index);
void     r_immediate_line(v3_t start, v3_t end, v3_t color);
void     r_immediate_flush(void);

typedef struct bitmap_font_t
{
    unsigned w, h, cw, ch;
    resource_handle_t texture;
} bitmap_font_t;

void r_immediate_text(const bitmap_font_t *font, v2_t p, v3_t color, string_t string);

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
void r_get_view(r_view_t *view);
void r_pop_view(void);

#endif /* RENDER_H */
