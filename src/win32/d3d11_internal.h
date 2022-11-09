#ifndef D3D11_INTERNAL_H
#define D3D11_INTERNAL_H

#include "core/api_types.h"
#include "render/render.h"

typedef struct d3d_cbuffer_t
{
    m4x4_t camera_projection;
    m4x4_t light_projection;
    m4x4_t model_transform;
    uint32_t frame_index;
    float    depth_bias;
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

typedef struct d3d_texture_t
{
    uint32_t flags;
    ID3D11Texture2D          *tex[6];
    ID3D11ShaderResourceView *srv;
} d3d_texture_t;

extern bulk_t d3d_models;
extern bulk_t d3d_textures;

typedef enum d3d_sampler_t
{
    D3D_SAMPLER_POINT,
    D3D_SAMPLER_LINEAR,
    D3D_SAMPLER_POINT_CLAMP,
    D3D_SAMPLER_LINEAR_CLAMP,
    D3D_SAMPLER_COUNT,
} d3d_sampler_t;

typedef struct d3d_state_t
{
    d3d_texture_t *white_texture;
    d3d_texture_t *missing_texture;
    d3d_texture_t *missing_texture_cubemap;

    d3d_model_t *skybox_model;

    int current_width, current_height;

    ID3D11Device             *device;
    ID3D11DeviceContext      *context;
    IDXGISwapChain1          *swap_chain;

    ID3D11Buffer             *immediate_ibuffer;
    ID3D11Buffer             *immediate_vbuffer;

    ID3D11SamplerState       *samplers[D3D_SAMPLER_COUNT];

    ID3D11BlendState         *bs;
    ID3D11RasterizerState    *rs;
    ID3D11RasterizerState    *rs_no_cull;
    ID3D11DepthStencilState  *dss;
    ID3D11DepthStencilState  *dss_no_depth;
    ID3D11Texture2D          *rt_tex;
    ID3D11RenderTargetView   *rt_rtv;
    ID3D11Texture2D          *msaa_rt_tex;
    ID3D11RenderTargetView   *msaa_rt_rtv;
    ID3D11ShaderResourceView *msaa_rt_srv;
    ID3D11DepthStencilView   *ds_view;

    ID3D11InputLayout        *layouts[VERTEX_FORMAT_COUNT];

    ID3D11Buffer             *ubuffer;
    ID3D11VertexShader       *world_vs;
    ID3D11PixelShader        *world_ps;
    ID3D11VertexShader       *immediate_vs;
    ID3D11PixelShader        *immediate_ps;
    ID3D11VertexShader       *skybox_vs;
    ID3D11PixelShader        *skybox_ps;
    ID3D11VertexShader       *msaa_resolve_vs;
    ID3D11PixelShader        *msaa_resolve_ps;

    uint32_t frame_index;
} d3d_state_t;

extern d3d_state_t d3d;

resource_handle_t upload_texture(const upload_texture_t *params);
void destroy_texture(resource_handle_t handle);

resource_handle_t upload_model(const upload_model_t *params);
void destroy_model(resource_handle_t model);

ID3DBlob *compile_shader(string_t hlsl_file, string_t hlsl, const char *entry_point, const char *kind);
ID3D11PixelShader *compile_ps(string_t hlsl_file, string_t hlsl, const char *entry_point);
ID3D11VertexShader *compile_vs(string_t hlsl_file, string_t hlsl, const char *entry_point);

void update_buffer(ID3D11Buffer *buffer, const void *data, size_t size);
void set_model_buffers(d3d_model_t *model);

void get_resolution(int *w, int *h);

typedef struct render_pass_t
{
    d3d_model_t *model;

    UINT cbuffer_count;
    ID3D11Buffer **cbuffers;

    ID3D11VertexShader *vs;
    ID3D11PixelShader  *ps;

    UINT srv_count;
    ID3D11ShaderResourceView **srvs;

    bool depth;
    bool cull;
    bool sample_linear;

    D3D11_VIEWPORT viewport;
} render_pass_t;

void render_model(const render_pass_t *pass);

#endif /* D3D11_INTERNAL_H */
