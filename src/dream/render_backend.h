#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

//
// texture stuff
//

enum { R_MISSING_TEXTURE_SIZE = 64 };
enum { R_RENDER_COMMAND_ALIGN = 16 };

// render limits
enum {
    R_VIEWS_CAPACITY              = 16,
    R_COMMANDS_CAPACITY           = 1 << 16,
    R_COMMANDS_DATA_CAPACITY      = MB(16),
    R_IMMEDIATE_INDICES_CAPACITY  = 1 << 20,
    R_IMMEDIATE_VERTICES_CAPACITY = 1 << 18,
    R_UI_RECTS_CAPACITY           = 8192 << 1,
};

typedef enum r_pixel_format_t
{
    R_PIXEL_FORMAT_R8,
    R_PIXEL_FORMAT_RG8,
    R_PIXEL_FORMAT_RGBA8,
    R_PIXEL_FORMAT_SRGB8_A8,
    R_PIXEL_FORMAT_R11G11B10F,
    R_PIXEL_FORMAT_R32G32B32F,
    R_PIXEL_FORMAT_R32G32B32A32F,
} r_pixel_format_t;

typedef enum r_texture_flags_t
{
    R_TEXTURE_FLAG_CUBEMAP = 0x1,
} texture_flags_t;

typedef enum r_texture_type_t
{
    R_TEXTURE_TYPE_2D,
    R_TEXTURE_TYPE_3D,
} r_texture_type_t;

typedef struct r_texture_desc_t
{
    r_texture_type_t type;
    r_pixel_format_t format;
    uint32_t w, h, d;
    uint32_t flags;
} r_texture_desc_t;

typedef struct r_texture_data_t
{
    uint32_t pitch;
    uint32_t slice_pitch;
    union
    {
        void *pixels;
        void *faces[6];
    };
} r_texture_data_t;

typedef enum r_upload_texture_flags_t
{
	R_UPLOAD_TEXTURE_GEN_MIPMAPS = 0x1, // NOTE: Causes synchronization with the main thread for concurrent access to ID3D11DeviceContext. Yucky.
} r_upload_texture_flags_t;

typedef struct r_upload_texture_t
{
	string_t debug_name;

	uint32_t upload_flags;
    r_texture_desc_t desc;
    r_texture_data_t data;
} r_upload_texture_t;

//
// mesh stuff
//

typedef enum r_topology_t
{
    R_TOPOLOGY_TRIANGLELIST,
    R_TOPOLOGY_TRIANGLESTRIP,
    R_TOPOLOGY_LINELIST,
    R_TOPOLOGY_LINESTRIP,
    R_TOPOLOGY_POINTLIST,
} r_topology_t;

typedef enum r_vertex_format_t
{
    R_VERTEX_FORMAT_POS,
    R_VERTEX_FORMAT_IMMEDIATE,
    R_VERTEX_FORMAT_BRUSH,
    R_VERTEX_FORMAT_COUNT,
} r_vertex_format_t;

extern uint32_t r_vertex_format_size[R_VERTEX_FORMAT_COUNT];

typedef struct r_vertex_pos_t
{
    v3_t pos;
} r_vertex_pos_t;

// TODO: Compact
typedef struct r_vertex_immediate_t
{
    v3_t pos;          // 12
    v2_t tex;          // 20
    uint32_t col;      // 24
	v3_t normal;       // 36
} r_vertex_immediate_t;

typedef struct r_vertex_brush_t
{
    v3_t pos;
    v2_t tex;
    v2_t tex_lightmap;
    v3_t normal;
} r_vertex_brush_t;

typedef struct r_upload_mesh_t
{
    r_topology_t topology;

    r_vertex_format_t vertex_format;
    uint32_t vertex_count;
    const void *vertices;

    uint32_t index_count;
    const uint16_t *indices;
} r_upload_mesh_t;

//
// timestamps
//

#define r_timestamp_t(_)                                        \
    _(RENDER_TS_BEGIN_FRAME, "begin frame")                     \
    _(RENDER_TS_CLEAR_SHADOWMAP, "clear shadowmap")             \
    _(RENDER_TS_RENDER_SHADOWMAP, "render shadowmap")           \
    _(RENDER_TS_CLEAR_MAIN, "clear main")                       \
    _(RENDER_TS_UPLOAD_IMMEDIATE_DATA, "upload immediate data") \
    _(RENDER_TS_SCENE, "render scene")                          \
    _(RENDER_TS_MSAA_RESOLVE, "msaa resolve")                   \
    _(RENDER_TS_BLOOM, "bloom")                                 \
    _(RENDER_TS_POST_PASS, "post-pass")                         \
    _(RENDER_TS_UI, "ui")                                       \
    _(RENDER_TS_END_FRAME, "end frame")                         \
    _(RENDER_TS_COUNT, "RENDER_TS_COUNT")                       \

#define DEFINE_ENUM(e, ...) e,

typedef enum r_timestamp_t
{
    r_timestamp_t(DEFINE_ENUM)
} r_timestamp_t;

#define DEFINE_NAME(e, name) [e] = name,

static const char *r_timestamp_names[] = {
    r_timestamp_t(DEFINE_NAME)
};

#undef DEFINE_ENUM
#undef DEFINE_NAME
#undef r_timestamp_t

typedef struct r_timings_t
{
    float dt[RENDER_TS_COUNT];
} r_timings_t;

//
// views
//

typedef unsigned char r_view_index_t;

typedef struct r_scene_parameters_t
{
    texture_handle_t  skybox;
    v3_t              sun_direction;
    v3_t              sun_color;

    texture_handle_t  fogmap;
    v3_t              fog_offset;
    v3_t              fog_dim;
    float             fog_density;
    float             fog_absorption;
    float             fog_scattering;
    float             fog_phase_k;
} r_scene_parameters_t;

typedef struct r_view_t
{
    bool              no_shadows      : 1;
    bool              no_post_process : 1;

    v3_t              camera_p;

    m4x4_t            view_matrix;
    m4x4_t            proj_matrix;

    rect2_t           clip_rect;

    r_scene_parameters_t scene;
} r_view_t;

//
// materials
//

typedef enum r_material_kind_t
{
    R_MATERIAL_NONE,

    R_MATERIAL_BRUSH,

    R_MATERIAL_COUNT,
} r_material_kind_t;

typedef enum r_material_flags_t
{
    R_MATERIAL_NO_SHADOW = 0x1,
} r_material_flags_t;

typedef struct r_material_t
{
    r_material_kind_t kind;
    uint32_t          id;
    uint32_t          flags;
} r_material_t;

typedef struct r_material_brush_t
{
    USING(r_material_t, base);

    texture_handle_t texture;
    texture_handle_t lightmap;
} r_material_brush_t;

//
// command stuff
//

typedef enum r_command_kind_t
{
    R_COMMAND_NONE,

    R_COMMAND_MODEL,
	R_COMMAND_UI_RECTS,   // FIXME: MEGA HACK. I re-ordered UI_RECTS to come before IMMEDIATE to make it look like my text rendering isn't busted.
    R_COMMAND_IMMEDIATE,
    
    R_COMMAND_COUNT,
} r_command_kind_t;

typedef struct r_command_model_t
{
    m4x4_t transform;

    mesh_handle_t mesh;
    const r_material_t *material;
} r_command_model_t;

// TODO: Use with ui rects
typedef struct r_rect2_fixed_t
{
    int16_t min_x; // 14:2 fixed point
    int16_t min_y; // 14:2 fixed point
    int16_t max_x; // 14:2 fixed point
    int16_t max_y; // 14:2 fixed point
} r_rect2_fixed_t;

typedef enum r_ui_rect_flags_t
{
    R_UI_RECT_BLEND_TEXT = 1 << 0,
} r_ui_rect_flags_t;

typedef struct r_ui_rect_t
{
	rect2_t  rect;              // 16
    rect2_t  tex_coords;        // 32
	v4_t     roundedness;       // 48
	uint32_t color_00;          // 52
	uint32_t color_10;          // 58
	uint32_t color_11;          // 64
	uint32_t color_01;          // 68
	float    shadow_radius;     // 74
	float    shadow_amount;     // 80
	float    inner_radius;      // 84
	uint32_t flags;             // 88
	float    pad1;              // 92
	float    pad2;              // 96
} r_ui_rect_t;

typedef struct r_command_ui_rects_t
{
    texture_handle_t texture;
	uint32_t first;
	uint32_t count;
} r_command_ui_rects_t;

typedef enum r_blend_mode_t
{
    R_BLEND_PREMUL_ALPHA,
    R_BLEND_ADDITIVE,
} r_blend_mode_t;

typedef enum r_cull_mode_t
{
	R_CULL_NONE,
	R_CULL_BACK,
	R_CULL_FRONT,
	R_CULL_COUNT,
} r_cull_mode_t;

typedef enum r_view_layer_t
{
    R_VIEW_LAYER_SCENE,
    R_VIEW_LAYER_UI,
} r_view_layer_t;

typedef enum r_screen_layer_t
{
    R_SCREEN_LAYER_SCENE,
    R_SCREEN_LAYER_UI,
} r_screen_layer_t;

typedef union r_command_key_t
{
    struct
    {
        uint64_t material_id  : 30;  // 30
        uint64_t depth        : 20;  // 50
        uint64_t kind         : 4;   // 54
        uint64_t view_layer   : 2;   // 56
        uint64_t view         : 6;   // 62
        uint64_t screen_layer : 2;   // 64
    };
    uint64_t value;
} r_command_key_t;

#define R_COMMAND_DEPTH_FAR_PLANE 10000.0f

DREAM_LOCAL uint64_t r_encode_command_depth(float depth);

typedef struct r_command_t
{
    r_command_key_t key;
    void           *data;
#if DREAM_SLOW
    string_t identifier;
#endif
} r_command_t;

typedef struct r_command_buffer_t
{
#if DREAM_SLOW
    arena_t debug_render_data_arena; // exists to avoid pushing more onto the data buffer, which would make its size requirements change based on builds which seems lame
#endif
    uint32_t     views_capacity;
    uint32_t     views_count;
    r_view_t    *views;

    uint32_t     commands_capacity;
    uint32_t     commands_count;
    r_command_t *commands;

    uint32_t     data_capacity;
    uint32_t     data_at;
    char        *data;

    uint32_t     imm_indices_capacity;
    uint32_t     imm_indices_count;
    uint32_t    *imm_indices;

    uint32_t     imm_vertices_capacity;
    uint32_t     imm_vertices_count;
    r_vertex_immediate_t *imm_vertices;

    uint32_t     ui_rects_capacity;
    uint32_t     ui_rects_count;
    r_ui_rect_t *ui_rects;
} r_command_buffer_t;

//
// immediate mode drawing
//

typedef enum r_immediate_shader_t
{
	R_SHADER_FLAT,
	R_SHADER_DEBUG_LIGHTING,
	R_SHADER_TEXT,
	R_SHADER_COUNT,
} r_immediate_shader_t;

typedef struct r_immediate_t
{
	r_immediate_shader_t shader;
    r_topology_t         topology;
    r_blend_mode_t       blend_mode;
	r_cull_mode_t        cull_mode;

    m4x4_t transform;

    rect2_t clip_rect;

    texture_handle_t texture;

    bool  use_depth;
    float depth_bias;

    uint32_t ioffset;
    uint32_t icount;

    uint32_t vcount;
    uint32_t voffset;
} r_immediate_t;

typedef struct render_api_i
{
    void                (*get_resolution)(int *w, int *h);

    bool                (*describe_texture)(texture_handle_t handle, r_texture_desc_t *desc);

	texture_handle_t    (*reserve_texture)(void); // @threadsafe
    // TODO(daniel): Way to wait on texture being populated?
	void                (*populate_texture)(texture_handle_t handle, const r_upload_texture_t *params); // @threadsafe (per handle)

    texture_handle_t    (*upload_texture)(const r_upload_texture_t *params);
    void                (*destroy_texture)(texture_handle_t texture);

    mesh_handle_t       (*upload_mesh)(const r_upload_mesh_t *params);
    void                (*destroy_mesh)(mesh_handle_t mesh);

    void                (*get_timings)(r_timings_t *timings);
} render_api_i;

void equip_render_api(render_api_i *api);
extern const render_api_i *const render;

#endif /* RENDER_BACKEND_H */
