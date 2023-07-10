#ifndef D3D11_INTERNAL_H
#define D3D11_INTERNAL_H

#include "core/api_types.h"
#include "core/thread.h"
#include "render/render.h"
#include "game/asset.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define D3D_SAFE_RELEASE(x) if (x) { IUnknown_Release((IUnknown *)(x)); (x) = NULL; }

typedef struct d3d_cbuffer_t
{
    m4x4_t view_matrix;
    m4x4_t proj_matrix;
    m4x4_t model_matrix;
    m4x4_t sun_matrix;
    v3_t     light_direction;
    float    pad0;
    uint32_t frame_index;
    float    depth_bias;
    float    pad1;
    float    pad2;
    v3_t     fog_offset;
    float    pad3;
    v3_t     fog_dim;
    float    pad4;
    v3_t     sun_color;
    float    pad5;
    v3_t     sun_direction;
    float    fog_density;
    float    fog_absorption;
    float    fog_scattering;
    float    fog_phase_k;
} d3d_cbuffer_t;

typedef struct d3d_model_t
{
    vertex_format_t vertex_format;

    D3D11_PRIMITIVE_TOPOLOGY primitive_topology;

    uint32_t vcount;
    ID3D11Buffer *vbuffer;
    uint32_t icount;
    ID3D11Buffer *ibuffer;
} d3d_model_t;

typedef enum d3d_texture_state_t
{
	D3D_TEXTURE_STATE_NONE,
	D3D_TEXTURE_STATE_RESERVED,
	D3D_TEXTURE_STATE_LOADING,
	D3D_TEXTURE_STATE_LOADED,
	D3D_TEXTURE_STATE_COUNT,
} d3d_texture_state_t;

typedef struct d3d_texture_t
{
    texture_desc_t desc;

	uint32_t state;

    union
    {
        ID3D11Texture2D *tex[6];
        ID3D11Texture3D *tex_3d;
    };
    ID3D11ShaderResourceView *srv;
} d3d_texture_t;

DREAM_API bulk_t d3d_models;
DREAM_API bulk_t d3d_textures;

typedef enum d3d_sampler_t
{
    D3D_SAMPLER_POINT,
    D3D_SAMPLER_LINEAR,
    D3D_SAMPLER_POINT_CLAMP,
    D3D_SAMPLER_LINEAR_CLAMP,
    D3D_SAMPLER_FOG,
    D3D_SAMPLER_SHADOWMAP,
    D3D_SAMPLER_COUNT,
} d3d_sampler_t;

typedef struct d3d_create_rendertarget_t
{
    UINT        w, h;
    DXGI_FORMAT format;
    int         msaa_samples;
    bool        create_color;
    bool        create_depth;
    bool        create_srv;
    bool        allow_color_mipmaps;
    bool        allow_depth_mipmaps;
} d3d_create_rendertarget_t;

typedef struct d3d_rendertarget_t
{
    ID3D11Texture2D          *color_tex;
    ID3D11RenderTargetView   *color_rtv;
    ID3D11ShaderResourceView *color_srv;

    ID3D11Texture2D          *depth_tex;
    ID3D11DepthStencilView   *depth_dsv;
    ID3D11ShaderResourceView *depth_srv;
} d3d_rendertarget_t;

typedef struct d3d_gen_mipmaps_t
{
	struct d3d_gen_mipmaps_t *next;

	resource_handle_t handle;
    texture_desc_t    desc;
    texture_data_t    data;
} d3d_gen_mipmaps_t;

typedef struct d3d_queries_t
{
    ID3D11Query *ts[RENDER_TS_COUNT];
    ID3D11Query *disjoint;
} d3d_queries_t;

#define D3D_QUERY_FRAME_COUNT 3

typedef struct d3d_state_t
{
	arena_t gen_mipmaps_arena;
	d3d_gen_mipmaps_t *first_gen_mipmaps;

	SRWLOCK context_lock;

    d3d_texture_t *white_texture;
    d3d_texture_t *missing_texture;
    d3d_texture_t *missing_texture_cubemap;
    d3d_texture_t *blue_noise;

    d3d_model_t *skybox_model;

    int current_width, current_height;

    ID3D11Device             *device;
    ID3D11DeviceContext      *context;
    IDXGISwapChain1          *swap_chain;

    ID3D11Buffer             *immediate_ibuffer;
    ID3D11Buffer             *immediate_vbuffer;

    d3d_queries_t             queries[D3D_QUERY_FRAME_COUNT];
    int                       query_frame;
    int                       query_collect_frame;
    render_timings_t          timings;

    ID3D11SamplerState       *samplers[D3D_SAMPLER_COUNT];

    ID3D11BlendState         *bs;
    ID3D11BlendState         *bs_additive;
    ID3D11RasterizerState    *rs;
    ID3D11RasterizerState    *rs_cull_front;
    ID3D11RasterizerState    *rs_no_cull;
    ID3D11DepthStencilState  *dss;
    ID3D11DepthStencilState  *dss_no_depth;
    ID3D11DepthStencilState  *dss_dont_write_depth;
    ID3D11DepthStencilState  *dss_less;
    ID3D11Texture2D          *rt_tex;
    ID3D11RenderTargetView   *rt_rtv;
    ID3D11Texture2D          *msaa_rt_tex;
    ID3D11RenderTargetView   *msaa_rt_rtv;
    ID3D11ShaderResourceView *msaa_rt_srv;

    d3d_rendertarget_t        scene_target;
    d3d_rendertarget_t        post_target;
    d3d_rendertarget_t        backbuffer;

    d3d_rendertarget_t        sun_shadowmap;

    ID3D11InputLayout        *layouts[VERTEX_FORMAT_COUNT];

    ID3D11Buffer             *ubuffer;
    ID3D11VertexShader       *shadowmap_vs;
    ID3D11VertexShader       *world_vs;
    ID3D11PixelShader        *world_ps;
    ID3D11VertexShader       *immediate_vs;
    ID3D11PixelShader        *immediate_ps;
    ID3D11VertexShader       *skybox_vs;
    ID3D11PixelShader        *skybox_ps;
    ID3D11VertexShader       *postprocess_vs;
    ID3D11PixelShader        *msaa_resolve_ps;
    ID3D11PixelShader        *hdr_resolve_ps;

    ID3D11Texture3D          *fog_map;
    ID3D11ShaderResourceView *fog_map_srv;

    uint32_t frame_index;
} d3d_state_t;

DREAM_API d3d_state_t d3d;

DREAM_INLINE void d3d_timestamp(render_timestamp_t ts)
{
    debug_print("Collecting ts: %s\n", render_timestamp_names[ts]);
	ID3D11DeviceContext_End(d3d.context, (ID3D11Asynchronous *)d3d.queries[d3d.query_frame].ts[ts]);
}

DREAM_API resource_handle_t upload_texture(const upload_texture_t *params);
DREAM_API void destroy_texture(resource_handle_t handle);

DREAM_API resource_handle_t upload_model(const upload_model_t *params);
DREAM_API void destroy_model(resource_handle_t model);

DREAM_API ID3DBlob *compile_shader(string_t hlsl_file, string_t hlsl, const char *entry_point, const char *kind);
DREAM_API ID3D11PixelShader *compile_ps(string_t hlsl_file, string_t hlsl, const char *entry_point);
DREAM_API ID3D11VertexShader *compile_vs(string_t hlsl_file, string_t hlsl, const char *entry_point);

DREAM_API void update_buffer(ID3D11Buffer *buffer, const void *data, size_t size);
DREAM_API void set_model_buffers(d3d_model_t *model, DXGI_FORMAT index_format);

DREAM_API void get_resolution(int *w, int *h);

typedef enum d3d_cull_mode_t
{
    D3D_CULL_NONE,
    D3D_CULL_BACK,
    D3D_CULL_FRONT,
} d3d_cull_mode_t;

typedef enum d3d_depth_test_t
{
    D3D_DEPTH_TEST_GREATER,
    D3D_DEPTH_TEST_LESS,
    D3D_DEPTH_TEST_NONE,
} d3d_depth_test_t;

typedef struct d3d_render_pass_t
{
    ID3D11RenderTargetView *render_target;
    ID3D11DepthStencilView *depth_stencil;

    d3d_model_t *model;

    r_blend_mode_t blend_mode;

    UINT cbuffer_count;
    ID3D11Buffer **cbuffers;

    ID3D11VertexShader *vs;
    ID3D11PixelShader  *ps;

    UINT srv_count;
    ID3D11ShaderResourceView **srvs;

    DXGI_FORMAT index_format;

    uint32_t ioffset;
    uint32_t voffset;

    d3d_depth_test_t depth;
    d3d_cull_mode_t cull;
    bool sample_linear;
    bool scissor;

    D3D11_VIEWPORT viewport;
    D3D11_RECT     scissor_rect;
} render_pass_t;

typedef struct d3d_post_pass_t
{
    ID3D11RenderTargetView *render_target;
    ID3D11PixelShader *ps;

    UINT srv_count;
    ID3D11ShaderResourceView **srvs;

    D3D11_VIEWPORT viewport;
    D3D11_RECT     scissor_rect;
} d3d_post_pass_t;

DREAM_API void render_model(const render_pass_t *pass);

DREAM_API d3d_rendertarget_t d3d_create_rendertarget(const d3d_create_rendertarget_t *params);
DREAM_API void d3d_release_rendertarget(d3d_rendertarget_t *rt);

#endif /* D3D11_INTERNAL_H */
