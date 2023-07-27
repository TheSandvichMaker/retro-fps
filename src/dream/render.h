#ifndef RENDER_H
#define RENDER_H

#include "core/core.h"

typedef struct render_settings_t
{
    int render_w, render_h;
    int msaa_level;
    int rendertarget_precision; // 0 = default, 1 = high precision

    int sun_shadowmap_resolution;
    bool use_dynamic_sun_shadows;

    uint32_t version;
} render_settings_t;

#define RENDER_COMMAND_ALIGN 16

#define R_MAX_IMMEDIATE_INDICES  (1 << 24)
#define R_MAX_IMMEDIATE_VERTICES (1 << 23)
#define R_MAX_UI_RECTS           4096

typedef struct bitmap_font_t
{
    unsigned w, h, cw, ch;
    resource_handle_t texture;
} bitmap_font_t;

extern v3_t g_debug_colors[6];

static inline v3_t r_debug_color(size_t i)
{
    return g_debug_colors[i % ARRAY_COUNT(g_debug_colors)];
}

typedef enum pixel_format_t
{
    PIXEL_FORMAT_R8,
    PIXEL_FORMAT_RG8,
    PIXEL_FORMAT_RGBA8,
    PIXEL_FORMAT_SRGB8_A8,
    PIXEL_FORMAT_R11G11B10F,
    PIXEL_FORMAT_R32G32B32F,
    PIXEL_FORMAT_R32G32B32A32F,
} pixel_format_t;

typedef enum texture_flags_t
{
    TEXTURE_FLAG_CUBEMAP = 0x1,
} texture_flags_t;

typedef enum texture_type_t
{
    TEXTURE_TYPE_2D,
    TEXTURE_TYPE_3D,
} texture_type_t;

typedef struct texture_desc_t
{
    texture_type_t type;
    pixel_format_t format;
    uint32_t w, h, d;
    uint32_t flags;
} texture_desc_t;

typedef struct texture_data_t
{
    uint32_t pitch;
    uint32_t slice_pitch;
    union
    {
        void *pixels;
        void *faces[6];
    };
} texture_data_t;

typedef enum upload_texture_flags_t
{
	UPLOAD_TEXTURE_GEN_MIPMAPS = 0x1, // NOTE: Causes synchronization with the main thread for concurrent access to ID3D11DeviceContext. Generating mips at runtime is yucky.
} upload_texture_flags_t;

typedef struct upload_texture_t
{
	uint32_t upload_flags;
    texture_desc_t desc;
    texture_data_t data;
} upload_texture_t;

typedef enum vertex_format_t
{
    VERTEX_FORMAT_POS,
    VERTEX_FORMAT_IMMEDIATE,
    VERTEX_FORMAT_BRUSH,
    VERTEX_FORMAT_COUNT,
} vertex_format_t;

typedef enum r_blend_mode_t
{
    R_BLEND_PREMUL_ALPHA,
    R_BLEND_ADDITIVE,
} r_blend_mode_t;

typedef enum r_topology_t
{
    R_TOPOLOGY_TRIANGLELIST, // default
    R_TOPOLOGY_TRIANGLESTRIP,
    R_TOPOLOGY_LINELIST,
    R_TOPOLOGY_LINESTRIP,
    R_TOPOLOGY_POINTLIST,
} r_topology_t;

typedef enum r_cull_mode_t
{
	R_CULL_NONE,
	R_CULL_BACK,
	R_CULL_FRONT,
	R_CULL_COUNT,
} r_cull_mode_t;

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
	v3_t normal;
} vertex_immediate_t;

typedef struct vertex_brush_t
{
    v3_t pos;
    v2_t tex;
    v2_t tex_lightmap;
    v3_t normal;
} vertex_brush_t;

typedef struct upload_model_t
{
    r_topology_t topology;

    vertex_format_t vertex_format;
    uint32_t vertex_count;
    const void *vertices;

    uint32_t index_count;
    const uint16_t *indices;
} upload_model_t;

enum { MISSING_TEXTURE_SIZE = 64 };

#define render_timestamp_t(_)                                   \
    _(RENDER_TS_BEGIN_FRAME, "begin frame")                     \
    _(RENDER_TS_CLEAR_SHADOWMAP, "clear shadowmap")             \
    _(RENDER_TS_RENDER_SHADOWMAP, "render shadowmap")           \
    _(RENDER_TS_CLEAR_MAIN, "clear main")                       \
    _(RENDER_TS_RENDER_SKYBOX, "render skybox")                 \
    _(RENDER_TS_UPLOAD_IMMEDIATE_DATA, "upload immediate data") \
    _(RENDER_TS_SCENE, "render scene")                          \
    _(RENDER_TS_MSAA_RESOLVE, "msaa resolve")                   \
    _(RENDER_TS_BLOOM, "bloom")                                 \
    _(RENDER_TS_POST_PASS, "post-pass")                         \
    _(RENDER_TS_UI, "ui")                                       \
    _(RENDER_TS_END_FRAME, "end frame")                         \
    _(RENDER_TS_COUNT, "RENDER_TS_COUNT")                       \

#define DEFINE_ENUM(e, ...) e,

typedef enum render_timestamp_t
{
    render_timestamp_t(DEFINE_ENUM)
} render_timestamp_t;

#define DEFINE_NAME(e, name) [e] = name,

static const char *render_timestamp_names[] = {
    render_timestamp_t(DEFINE_NAME)
};

#undef DEFINE_ENUM
#undef DEFINE_NAME
#undef render_timestamp_t

typedef struct render_timings_t
{
    float dt[RENDER_TS_COUNT];
} render_timings_t;

typedef struct render_api_i
{
    void (*get_resolution)(int *w, int *h);

    // TODO: Querying this from the backend seems pointless, just remember it on the user side...
    bool (*describe_texture)(resource_handle_t handle, texture_desc_t *desc);

	// FIXME: Thread safe, but right now doesn't provide a synchronization option.
	// When the texture is not yet done loading, it will instead show as a missing texture.
	// Should be fixed!
	resource_handle_t (*reserve_texture)(void);
	void (*populate_texture)(resource_handle_t handle, const upload_texture_t *params);
    resource_handle_t (*upload_texture)(const upload_texture_t *params); // implemented in terms of reserve + populate, I presume
    void (*destroy_texture)(resource_handle_t texture);

    resource_handle_t (*upload_model)(const upload_model_t *params);
    void (*destroy_model)(resource_handle_t model);

    void (*get_timings)(render_timings_t *timings);
} render_api_i;

void equip_render_api(render_api_i *api);
extern const render_api_i *const render;

typedef struct r_view_t
{
    v3_t camera_p;
    rect2_t clip_rect;
    m4x4_t camera, projection;
    resource_handle_t skybox;
    resource_handle_t fogmap;
    v3_t fog_offset;
    v3_t fog_dim;
    v3_t sun_color;
    float fog_density;
    float fog_absorption;
    float fog_scattering;
    float fog_phase_k;
} r_view_t;

typedef enum r_command_kind_t
{
    R_COMMAND_NONE, // is here just to catch mistake of not initializing the kind 

    R_COMMAND_MODEL,
    R_COMMAND_IMMEDIATE,
	R_COMMAND_UI_RECTS,
    R_COMMAND_END_SCENE_PASS,
    
    R_COMMAND_COUNT,
} r_command_kind_t;

typedef enum r_render_stage_t
{
    R_RENDER_STAGE_SCENE,
    R_RENDER_STAGE_UI,
} r_render_stage_t;

typedef enum r_render_flags_t
{
    R_RENDER_FLAG_NO_SHADOW = 0x1,
} r_render_flags_t;

typedef struct r_command_base_t
{
    unsigned char  kind;
    unsigned char  view;
    unsigned short flags;
#ifndef NDEBUG
    string_t identifier;
#endif
} r_command_base_t;

typedef struct r_command_model_t
{
    r_command_base_t base;

    m4x4_t transform;

    resource_handle_t model;
    resource_handle_t texture;
    resource_handle_t lightmap;
} r_command_model_t;

typedef struct r_ui_rect_t
{
	rect2_t  rect;              // 16
	v4_t     roundedness;       // 32
	uint32_t color;             // 36
	float    shadow_radius;     // 40
	float    shadow_amount;     // 44
	float    pad0;              // 48
} r_ui_rect_t;

typedef struct r_command_ui_rects_t
{
	r_command_base_t base;
	uint32_t first;
	uint32_t count;
} r_command_ui_rects_t;

typedef enum r_immediate_shader_t
{
	R_SHADER_FLAT,
	R_SHADER_DEBUG_LIGHTING,
	R_SHADER_COUNT,
} r_immediate_shader_t;

typedef struct r_immediate_params_t
{
	r_immediate_shader_t shader;
    r_topology_t   topology;
    r_blend_mode_t blend_mode;
	r_cull_mode_t  cull_mode;

    rect2_t clip_rect;

    resource_handle_t texture;

    bool  depth_test;
    float depth_bias;
} r_immediate_params_t;

typedef struct r_immediate_draw_t
{
    r_immediate_params_t params;

    uint32_t ioffset;
    uint32_t icount;

    uint32_t vcount;
    uint32_t voffset;
} r_immediate_draw_t;

typedef struct r_command_immediate_t
{
    r_command_base_t base;
    r_immediate_draw_t draw_call;
} r_command_immediate_t;

DREAM_API void     r_immediate_shader    (r_immediate_shader_t shader);
DREAM_API void     r_immediate_topology  (r_topology_t topology);
DREAM_API void     r_immediate_blend_mode(r_blend_mode_t blend_mode);
DREAM_API void     r_immediate_cull_mode (r_cull_mode_t cull_mode);
DREAM_API void     r_immediate_clip_rect (rect2_t clip_rect); // TODO: Why does this exist?
DREAM_API void     r_immediate_texture   (resource_handle_t texture);
DREAM_API void     r_immediate_use_depth (bool depth);
DREAM_API void     r_immediate_depth_bias(float depth_bias);

DREAM_API uint32_t r_immediate_vertex    (const vertex_immediate_t *vertex);
DREAM_API void     r_immediate_index     (uint32_t index);

DREAM_API void     r_immediate_flush     (void);

DREAM_API void     r_ui_rect(r_ui_rect_t rect);

enum { R_MAX_VIEWS = 128 };
typedef unsigned char r_view_index_t;

typedef struct r_list_t
{
    r_view_index_t view_count;
    r_view_t views[R_MAX_VIEWS];

    r_view_index_t view_stack_at;
    r_view_index_t view_stack[R_MAX_VIEWS];

    size_t command_list_size;
    char *command_list_base;
    char *command_list_at;

    r_immediate_draw_t curr_immediate;

    uint32_t max_immediate_icount;
    uint32_t immediate_icount;
    uint32_t *immediate_indices;

    uint32_t max_immediate_vcount;
    uint32_t immediate_vcount;
    vertex_immediate_t *immediate_vertices;

	r_command_ui_rects_t *current_ui_rects;

	uint32_t max_ui_rect_count;
	uint32_t ui_rect_count;
	r_ui_rect_t *ui_rects;
} r_list_t;

void r_command_identifier(string_t identifier);
void r_draw_model(m4x4_t transform, resource_handle_t model, resource_handle_t texture, resource_handle_t lightmap);
void r_end_scene_pass(void);

void r_set_command_list(r_list_t *list);
void r_reset_command_list(void);

void r_default_view(r_view_t *view);
void r_push_view(const r_view_t *view);
void r_push_view_screenspace(void);
void r_push_view_screenspace_clip_rect(rect2_t clip_rect);
void r_copy_top_view(r_view_t *view);
r_view_t *r_get_top_view(void);
v3_t r_to_view_space(const r_view_t *view, v3_t p, float w);
void r_pop_view(void);

#endif /* RENDER_H */
