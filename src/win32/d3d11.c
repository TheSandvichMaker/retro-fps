#define COBJMACROS
#define NOMINMAX

#pragma warning(push, 0)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <d3dcompiler.h>
#include <math.h>

#pragma warning(pop)

#include "d3d11.h"
#include "d3d11_internal.h"
#include "d3d11_data.h"

#ifndef NDEBUG
#define DEBUG_RENDERER
#endif

bulk_t d3d_models   = INIT_BULK_DATA(d3d_model_t);
bulk_t d3d_textures = INIT_BULK_DATA(d3d_texture_t);

d3d_state_t d3d;

int init_d3d11(void *hwnd_)
{
    HWND hwnd = hwnd_;

    HRESULT hr = S_OK;

    {
        UINT flags = 0;
#if defined(DEBUG_RENDERER)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_DRIVER_TYPE driver_types[] = {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP,
            D3D_DRIVER_TYPE_REFERENCE,
        };

        D3D_FEATURE_LEVEL levels[] = { 
            D3D_FEATURE_LEVEL_11_1, 
            D3D_FEATURE_LEVEL_11_0, 
        };

        D3D_FEATURE_LEVEL picked_level = 0;

        for (size_t i = 0; i < ARRAY_COUNT(driver_types); i++)
        {
            D3D_DRIVER_TYPE driver = driver_types[i];
            hr = D3D11CreateDevice(NULL, driver, NULL, flags, levels, ARRAY_COUNT(levels), D3D11_SDK_VERSION, &d3d.device, &picked_level, &d3d.context);

            if (hr == E_INVALIDARG)
            {
                // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it.
                hr = D3D11CreateDevice(NULL, driver, NULL, flags, levels + 1, ARRAY_COUNT(levels) - 1, D3D11_SDK_VERSION, &d3d.device, &picked_level, &d3d.context);
            }

            if (SUCCEEDED(hr))
            {
                break;
            }
        }
    }

    if (!d3d.device)   return -1;
    if (!d3d.context)  return -1;

#if defined(DEBUG_RENDERER)
    {
        // debug break on API errors
        ID3D11InfoQueue *info;
        ID3D11Device_QueryInterface(d3d.device, &IID_ID3D11InfoQueue, &info);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        ID3D11InfoQueue_Release(info);
    }
#endif

    // create swap chain
    {
        IDXGIFactory2 *factory;
        hr = CreateDXGIFactory(&IID_IDXGIFactory2, &factory);
        if (FAILED(hr))  return -1;

        DXGI_SWAP_CHAIN_DESC1 desc = {
            // default 0 value for width & height means to get it from HWND automatically
            //.Width = 0,
            //.Height = 0,        

            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,

            .SampleDesc = { 1, 0 },

            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 2,

            .Scaling = DXGI_SCALING_NONE,

            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        }; 

        IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)d3d.device, hwnd, &desc, NULL, NULL, &d3d.swap_chain);
        IDXGIFactory_Release(factory);
    }

    {
        // get backbuffer size
        ID3D11Texture2D* backbuffer;
        IDXGISwapChain1_GetBuffer(d3d.swap_chain, 0, &IID_ID3D11Texture2D, &backbuffer);

        D3D11_TEXTURE2D_DESC desc;
        ID3D11Texture2D_GetDesc(backbuffer, &desc);

        d3d.current_width  = desc.Width;
        d3d.current_height = desc.Height;

        ID3D11Texture2D_Release(backbuffer);
    }

    // disable alt+enter keybind
    {
        IDXGIFactory *factory;
        IDXGISwapChain1_GetParent(d3d.swap_chain, &IID_IDXGIFactory, &factory);
        IDXGIFactory_MakeWindowAssociation(factory, hwnd, DXGI_MWA_NO_ALT_ENTER);
        IDXGIFactory_Release(factory);
    }

    // create input layouts

    m_scoped(temp)
    {
        string_t hlsl_file = strlit("gamedata/shaders/input_layout_nonsense.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        ID3DBlob *pos         = compile_shader(hlsl_file, hlsl, "pos", "vs_5_0");
        ID3DBlob *pos_tex_col = compile_shader(hlsl_file, hlsl, "pos_tex_col", "vs_5_0");
        ID3DBlob *brush       = compile_shader(hlsl_file, hlsl, "brush", "vs_5_0");

        D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex_pos_tex_col_t, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex_pos_tex_col_t, tex), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex_pos_tex_col_t, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        ID3D11Device_CreateInputLayout(d3d.device, desc, 1, ID3D10Blob_GetBufferPointer(pos),         ID3D10Blob_GetBufferSize(pos),         &d3d.layouts[VERTEX_FORMAT_POS]);
        ID3D11Device_CreateInputLayout(d3d.device, desc, 3, ID3D10Blob_GetBufferPointer(pos_tex_col), ID3D10Blob_GetBufferSize(pos_tex_col), &d3d.layouts[VERTEX_FORMAT_POS_TEX_COL]);

        D3D11_INPUT_ELEMENT_DESC desc2[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex_brush_t, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex_brush_t, tex), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD_LIGHTMAP", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex_brush_t, tex_lightmap), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex_brush_t, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        ID3D11Device_CreateInputLayout(d3d.device, desc2, ARRAY_COUNT(desc2), ID3D10Blob_GetBufferPointer(brush), ID3D10Blob_GetBufferSize(brush), &d3d.layouts[VERTEX_FORMAT_BRUSH]);

        ID3D10Blob_Release(pos);
        ID3D10Blob_Release(pos_tex_col);
        ID3D10Blob_Release(brush);
    }

    // compile shaders

    m_scoped(temp)
    {
        string_t hlsl_file = strlit("gamedata/shaders/brushmodel.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.world_vs = compile_vs(hlsl_file, hlsl, "vs");
        d3d.world_ps = compile_ps(hlsl_file, hlsl, "ps");
    }

    m_scoped(temp)
    {
        string_t hlsl_file = strlit("gamedata/shaders/immediate.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.immediate_vs = compile_vs(hlsl_file, hlsl, "vs");
        d3d.immediate_ps = compile_ps(hlsl_file, hlsl, "ps");
    }

    m_scoped(temp)
    {
        string_t hlsl_file = strlit("gamedata/shaders/skybox.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.skybox_vs = compile_vs(hlsl_file, hlsl, "vs");
        d3d.skybox_ps = compile_ps(hlsl_file, hlsl, "ps");
    }

    m_scoped(temp)
    {
        string_t hlsl_file = strlit("gamedata/shaders/msaa_resolve.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.msaa_resolve_vs = compile_vs(hlsl_file, hlsl, "vs");
        d3d.msaa_resolve_ps = compile_ps(hlsl_file, hlsl, "ps");
    }

    // create constant buffers

    {
        D3D11_BUFFER_DESC desc = {
            .ByteWidth      = sizeof(d3d_cbuffer_t),
            .Usage          = D3D11_USAGE_DYNAMIC,
            .BindFlags      = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };
        ID3D11Device_CreateBuffer(d3d.device, &desc, NULL, &d3d.ubuffer);
    }

    // create samplers

    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter   = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
            .MaxLOD   = FLT_MAX,
        };
        ID3D11Device_CreateSamplerState(d3d.device, &desc, &d3d.samplers[D3D_SAMPLER_POINT]);
    }

    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter   = D3D11_FILTER_ANISOTROPIC,
            .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
            .MaxLOD   = FLT_MAX,
            .MaxAnisotropy = 16,
        };
        ID3D11Device_CreateSamplerState(d3d.device, &desc, &d3d.samplers[D3D_SAMPLER_LINEAR]);
    }

    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter   = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
            .MaxLOD   = FLT_MAX,
        };
        ID3D11Device_CreateSamplerState(d3d.device, &desc, &d3d.samplers[D3D_SAMPLER_POINT_CLAMP]);
    }

    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter   = D3D11_FILTER_ANISOTROPIC,
            .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
            .MaxLOD   = FLT_MAX,
            .MaxAnisotropy = 16,
        };
        ID3D11Device_CreateSamplerState(d3d.device, &desc, &d3d.samplers[D3D_SAMPLER_LINEAR_CLAMP]);
    }

    // enable alpha blending

    {
        D3D11_BLEND_DESC desc =
        {
            .RenderTarget[0] = {
                .BlendEnable           = TRUE,
                .SrcBlend              = D3D11_BLEND_SRC_ALPHA,
                .DestBlend             = D3D11_BLEND_INV_SRC_ALPHA,
                .BlendOp               = D3D11_BLEND_OP_ADD,
                .SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA,
                .DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA,
                .BlendOpAlpha          = D3D11_BLEND_OP_ADD,
                .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
            },
        };
        ID3D11Device_CreateBlendState(d3d.device, &desc, &d3d.bs);
    }

    // create rasterizer state

    {
        D3D11_RASTERIZER_DESC desc = {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_BACK,
        };
        ID3D11Device_CreateRasterizerState(d3d.device, &desc, &d3d.rs);
    }

    // create no-cull rasterizer state

    {
        D3D11_RASTERIZER_DESC desc = {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_NONE,
        };
        ID3D11Device_CreateRasterizerState(d3d.device, &desc, &d3d.rs_no_cull);
    }

    // create depth & stencil

    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable      = TRUE,
            .DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc        = D3D11_COMPARISON_GREATER,
            .StencilEnable    = FALSE,
            .StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK,
            .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
        };
        ID3D11Device_CreateDepthStencilState(d3d.device, &desc, &d3d.dss);
    }

    // create no-depth test depth & stencil

    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable      = FALSE,
            .DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc        = D3D11_COMPARISON_GREATER,
            .StencilEnable    = FALSE,
            .StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK,
            .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
        };
        ID3D11Device_CreateDepthStencilState(d3d.device, &desc, &d3d.dss_no_depth);
    }

    // white texture

    {
        uint32_t pixel = 0xFFFFFFFF;

        resource_handle_t handle = upload_texture(&(upload_texture_t){
            .format = PIXEL_FORMAT_RGBA8,
            .w      = 1,
            .h      = 1,
            .pixels = &pixel,
        });
        d3d.white_texture = bd_get(&d3d_textures, handle);
    }

    // default "missing texture" texture

    {
        enum { w = MISSING_TEXTURE_SIZE, h = MISSING_TEXTURE_SIZE };

        uint32_t pixels[w*h];

        for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++)
        {
            if (((x >> 4) & 1) ^
                ((y >> 4) & 1))
            {
                pixels[y*w + x] = 0xFFFF00FF;
            }
            else
            {
                pixels[y*w + x] = 0xFF000000;
            }
        }

        resource_handle_t handle = upload_texture(&(upload_texture_t){
            .format = PIXEL_FORMAT_RGBA8,
            .w      = w,
            .h      = h,
            .pixels = pixels,
        });
        d3d.missing_texture = bd_get(&d3d_textures, handle);

        resource_handle_t cubemap_handle = upload_texture(&(upload_texture_t){
            .format = PIXEL_FORMAT_RGBA8,
            .w      = w,
            .h      = h,
            .flags  = TEXTURE_FLAG_CUBEMAP,
            .faces  = {
                pixels,
                pixels,
                pixels,
                pixels,
                pixels,
                pixels,
            },
        });
        d3d.missing_texture_cubemap = bd_get(&d3d_textures, cubemap_handle);
    }

    // create immediate rendering buffers

    {
        size_t stride = sizeof(uint16_t);
        size_t icount = MAX_IMMEDIATE_INDICES;

        D3D11_BUFFER_DESC desc = {
            .ByteWidth      = (DWORD)(stride*icount),
            .Usage          = D3D11_USAGE_DYNAMIC,
            .BindFlags      = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(d3d.device, &desc, NULL, &d3d.immediate_ibuffer);
    }

    {
        size_t stride = vertex_format_size[VERTEX_FORMAT_POS_TEX_COL];
        size_t vcount = MAX_IMMEDIATE_VERTICES;

        D3D11_BUFFER_DESC desc = {
            .ByteWidth      = (DWORD)(stride*vcount),
            .Usage          = D3D11_USAGE_DYNAMIC,
            .BindFlags      = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(d3d.device, &desc, NULL, &d3d.immediate_vbuffer);
    }

    // create skybox model

    {
        resource_handle_t handle = upload_model(&(upload_model_t){
            .vertex_format = VERTEX_FORMAT_POS,
            .vertex_count  = ARRAY_COUNT(g_skybox_vertices) / 3,
            .vertices      = g_skybox_vertices,
        });
        d3d.skybox_model = bd_get(&d3d_models, handle);
    }

    return 0;
}

void render_model(const render_pass_t *pass)
{
    // set vertex buffer and friends
    set_model_buffers(pass->model);

    // set vertex shader
    ID3D11DeviceContext_VSSetConstantBuffers(d3d.context, 0, pass->cbuffer_count, pass->cbuffers);
    ID3D11DeviceContext_VSSetShader(d3d.context, pass->vs, NULL, 0);

    // set rasterizer state
    ID3D11DeviceContext_RSSetViewports(d3d.context, 1, &pass->viewport);
    ID3D11DeviceContext_RSSetState(d3d.context, pass->cull ? d3d.rs : d3d.rs_no_cull);

    // set pixel shader
    // ID3D11DeviceContext_PSSetSamplers(d3d.context, 0, 1, pass->sample_linear ? &d3d.linear_sampler : &d3d.point_sampler);
    ID3D11DeviceContext_PSSetSamplers(d3d.context, 0, D3D_SAMPLER_COUNT, d3d.samplers);
    ID3D11DeviceContext_PSSetShaderResources(d3d.context, 0, pass->srv_count, pass->srvs);
    ID3D11DeviceContext_PSSetShader(d3d.context, pass->ps, NULL, 0);

    // set output merger state
    ID3D11DeviceContext_OMSetBlendState(d3d.context, d3d.bs, NULL, ~0U);
    ID3D11DeviceContext_OMSetDepthStencilState(d3d.context, pass->depth ? d3d.dss : d3d.dss_no_depth, 0);
    ID3D11DeviceContext_OMSetRenderTargets(d3d.context, 1, &d3d.msaa_rt_rtv, d3d.ds_view);

    // draw 
    if (pass->model->icount > 0)
    {
        ID3D11DeviceContext_DrawIndexed(d3d.context, pass->model->icount, 0, 0);
    }
    else
    {
        ID3D11DeviceContext_Draw(d3d.context, pass->model->vcount, 0);
    }
}

void d3d11_ensure_swapchain_size(int width, int height)
{
    if (d3d.rt_rtv == NULL || width != d3d.current_width || height != d3d.current_height)
    {
        if (d3d.rt_rtv)
        {
            // release old swap chain buffers
            ID3D11DeviceContext_ClearState(d3d.context);
            ID3D11RenderTargetView_Release(d3d.rt_rtv);
            ID3D11Texture2D_Release(d3d.rt_tex);
            ID3D11Texture2D_Release(d3d.msaa_rt_tex);
            ID3D11RenderTargetView_Release(d3d.msaa_rt_rtv);
            ID3D11RenderTargetView_Release(d3d.msaa_rt_srv);
            ID3D11DepthStencilView_Release(d3d.ds_view);
            d3d.rt_rtv = NULL;
        }

        // resize to new size for non-zero size
        if (width != 0 && height != 0)
        {
            IDXGISwapChain1_ResizeBuffers(d3d.swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);

            // create RenderTarget view for new backbuffer texture
            IDXGISwapChain1_GetBuffer(d3d.swap_chain, 0, &IID_ID3D11Texture2D, &d3d.rt_tex);
            ID3D11Device_CreateRenderTargetView(d3d.device, (ID3D11Resource *)d3d.rt_tex, NULL, &d3d.rt_rtv);

            D3D11_TEXTURE2D_DESC msaa_rt_desc = {
                .Width      = width,
                .Height     = height,
                .MipLevels  = 1,
                .ArraySize  = 1,
                .Format     = DXGI_FORMAT_R8G8B8A8_UNORM,
                .SampleDesc = { 8, 0 },
                .Usage      = D3D11_USAGE_DEFAULT,
                .BindFlags  = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
            };

            ID3D11Device_CreateTexture2D(d3d.device, &msaa_rt_desc, NULL, &d3d.msaa_rt_tex);
            ID3D11Device_CreateRenderTargetView(d3d.device, (ID3D11Resource *)d3d.msaa_rt_tex, NULL, &d3d.msaa_rt_rtv);
            ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)d3d.msaa_rt_tex, NULL, &d3d.msaa_rt_srv);

            D3D11_TEXTURE2D_DESC depth_desc = {
                .Width      = width,
                .Height     = height,
                .MipLevels  = 1,
                .ArraySize  = 1,
                .Format     = DXGI_FORMAT_D32_FLOAT, // or use DXGI_FORMAT_D32_FLOAT_S8X24_UINT if you need stencil
                .SampleDesc = { 8, 0 },
                .Usage      = D3D11_USAGE_DEFAULT,
                .BindFlags  = D3D11_BIND_DEPTH_STENCIL,
            };

            // create new depth stencil texture & DepthStencil view
            ID3D11Texture2D* depth;
            ID3D11Device_CreateTexture2D(d3d.device, &depth_desc, NULL, &depth);
            ID3D11Device_CreateDepthStencilView(d3d.device, (ID3D11Resource*)depth, NULL, &d3d.ds_view);
            ID3D11Texture2D_Release(depth);
        }

        d3d.current_width = width;
        d3d.current_height = height;
    }
}

void d3d11_draw_list(r_list_t *list, int width, int height)
{
    d3d11_ensure_swapchain_size(width, height);

    if (d3d.rt_rtv)
    {
        D3D11_VIEWPORT viewport =
        {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width    = (FLOAT)width,
            .Height   = (FLOAT)height,
            .MinDepth = 0,
            .MaxDepth = 1,
        };

        // clear screen
        FLOAT color[] = { 0.0f, 0.4f, 0.3f, 1.f };
        ID3D11DeviceContext_ClearRenderTargetView(d3d.context, d3d.rt_rtv, color);
        ID3D11DeviceContext_ClearDepthStencilView(d3d.context, d3d.ds_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 0.0f, 0);

        // render any potential skyboxes
        for (size_t i = 1; i < list->view_count; i++)
        {
            r_view_t *view = &list->views[i];

            if (!RESOURCE_HANDLE_VALID(view->skybox))
                continue;

            m4x4_t camera = view->camera;

            camera.e[3][0] = 0.0f;
            camera.e[3][1] = 0.0f;
            camera.e[3][2] = 0.0f;

            m4x4_t swizzle = {
                1, 0, 0, 0,
                0, 0, 1, 0,
                0, 1, 0, 0,
                0, 0, 0, 1,
            };

            m4x4_t mvp = m4x4_identity;
            mvp = mul(mvp, view->projection);
            mvp = mul(mvp, camera);
            mvp = mul(mvp, swizzle);

            d3d_cbuffer_t cbuffer = {
                .camera_projection = mvp,
            };

            update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

            d3d_model_t   *model   = d3d.skybox_model;

            d3d_texture_t *texture = bd_get(&d3d_textures, view->skybox);
            if (!texture)  texture = d3d.missing_texture_cubemap;

            render_model(&(render_pass_t) {
                .model = model,

                .vs = d3d.skybox_vs,
                .ps = d3d.skybox_ps,

                .cbuffer_count = 1,
                .cbuffers      = (ID3D11Buffer *[]) { d3d.ubuffer },
                .srv_count     = 1,
                .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv, },

                .depth         = false,
                .cull          = false,
                .sample_linear = true,
                .viewport      = viewport,
            });
        }

        // meat and potatoes
        for (char *at = list->command_list_base; at < list->command_list_at;)
        {
            r_command_base_t *base = (r_command_base_t *)at;
            r_view_t *view = &list->views[base->view];

            switch (base->kind)
            {
                case R_COMMAND_MODEL:
                {
                    r_command_model_t *command = (r_command_model_t *)base;
                    at = align_address(at + sizeof(*command), RENDER_COMMAND_ALIGN);

                    d3d_model_t *model = bd_get(&d3d_models, command->model);
                    if (model)
                    {
                        d3d_texture_t *texture = bd_get(&d3d_textures, command->texture);
                        if (!texture)  texture = d3d.missing_texture;

                        d3d_texture_t *lightmap = bd_get(&d3d_textures, command->lightmap);
                        if (!lightmap) lightmap = d3d.white_texture;

                        m4x4_t camera_projection = m4x4_identity;
                        camera_projection = mul(camera_projection, view->projection);
                        camera_projection = mul(camera_projection, view->camera);

                        d3d_cbuffer_t cbuffer = {
                            .camera_projection = camera_projection,
                            .model_transform   = command->transform,
                        };

                        update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

                        render_model(&(render_pass_t) {
                            .model = model,

                            .vs = d3d.world_vs,
                            .ps = d3d.world_ps,

                            .cbuffer_count = 1,
                            .cbuffers      = (ID3D11Buffer *[]) { d3d.ubuffer },
                            .srv_count     = 2,
                            .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv, lightmap->srv, },

                            .depth         = true,
                            .cull          = true,
                            .sample_linear = false,
                            .viewport      = viewport,
                        });
                    }
                } break;

                case R_COMMAND_IMMEDIATE:
                {
                    r_command_immediate_t *command = (r_command_immediate_t *)base;
                    at = align_address(at + sizeof(*command), RENDER_COMMAND_ALIGN);

                    m4x4_t camera_projection = m4x4_identity;
                    camera_projection = mul(camera_projection, view->projection);
                    camera_projection = mul(camera_projection, view->camera);

                    d3d_cbuffer_t cbuffer = {
                        .camera_projection = camera_projection,
                        .model_transform   = command->transform,
                    };

                    update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

                    uint32_t icount = MIN(command->icount, MAX_IMMEDIATE_INDICES);
                    bool use_ibuffer = icount > 0;

                    if (use_ibuffer)
                        update_buffer(d3d.immediate_ibuffer, command->ibuffer, sizeof(command->ibuffer[0])*icount);

                    uint32_t vcount = MIN(command->vcount, MAX_IMMEDIATE_VERTICES);
                    update_buffer(d3d.immediate_vbuffer, command->vbuffer, sizeof(command->vbuffer[0])*vcount);

                    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                    switch (command->topology)
                    {
                        case R_PRIMITIVE_TOPOLOGY_TRIANGELIST: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
                        case R_PRIMITIVE_TOPOLOGY_LINELIST:    topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
                        case R_PRIMITIVE_TOPOLOGY_LINESTRIP:   topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
                        case R_PRIMITIVE_TOPOLOGY_POINTLIST:   topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
                        INVALID_DEFAULT_CASE;
                    }

                    d3d_model_t immediate_model = {
                        .vertex_format      = VERTEX_FORMAT_POS_TEX_COL,
                        .primitive_topology = topology,
                        .vcount             = vcount,
                        .vbuffer            = d3d.immediate_vbuffer,
                        .icount             = icount,
                        .ibuffer            = d3d.immediate_ibuffer,
                    };

                    d3d_texture_t *texture = bd_get(&d3d_textures, command->texture);
                    if (!texture)  texture = d3d.white_texture;

                    render_model(&(render_pass_t) {
                        .model = &immediate_model,

                        .vs = d3d.immediate_vs,
                        .ps = d3d.immediate_ps,

                        .cbuffer_count = 1,
                        .cbuffers      = (ID3D11Buffer *[]) { d3d.ubuffer },
                        .srv_count     = 1,
                        .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv },

                        .depth    = !command->no_depth_test,
                        .cull     = false,
                        .viewport = viewport,
                    });
                } break;

                INVALID_DEFAULT_CASE;
            }
        }

        // set output merger state
        ID3D11DeviceContext_OMSetBlendState(d3d.context, d3d.bs, NULL, ~0U);
        ID3D11DeviceContext_OMSetDepthStencilState(d3d.context, d3d.dss_no_depth, 0);
        ID3D11DeviceContext_OMSetRenderTargets(d3d.context, 1, &d3d.rt_rtv, NULL);

        // set vertex shader
        ID3D11DeviceContext_VSSetShader(d3d.context, d3d.msaa_resolve_vs, NULL, 0);

        // set rasterizer state
        ID3D11DeviceContext_RSSetViewports(d3d.context, 1, &viewport);
        ID3D11DeviceContext_RSSetState(d3d.context, d3d.rs_no_cull);

        // set pixel shader
        ID3D11DeviceContext_PSSetSamplers(d3d.context, 0, D3D_SAMPLER_COUNT, d3d.samplers);
        ID3D11DeviceContext_PSSetShaderResources(d3d.context, 0, 1, &d3d.msaa_rt_srv);
        ID3D11DeviceContext_PSSetShader(d3d.context, d3d.msaa_resolve_ps, NULL, 0);

        ID3D11DeviceContext_Draw(d3d.context, 3, 0);
    }

    // ID3D11DeviceContext_ResolveSubresource(d3d.context, (ID3D11Resource *)d3d.rt_tex, 0, (ID3D11Resource *)d3d.msaa_rt_tex, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
}

void d3d11_present()
{
    HRESULT hr = IDXGISwapChain_Present(d3d.swap_chain, 1, 0);

    if (hr == DXGI_STATUS_OCCLUDED)
        Sleep(16);
}

void get_resolution(int *w, int *h)
{
    *w = d3d.current_width;
    *h = d3d.current_height;
}

resource_handle_t upload_texture(const upload_texture_t *params)
{
    resource_handle_t result = { 0 };

    if (!params->pixels)
        return result;

    d3d_texture_t *resource = bd_add(&d3d_textures);
    if (resource)
    {
        resource->flags = params->flags;

        uint32_t pixel_size = 1;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        switch (params->format)
        {
            case PIXEL_FORMAT_R8:       pixel_size = 1; format = DXGI_FORMAT_R8_UNORM;            break;
            case PIXEL_FORMAT_RG8:      pixel_size = 2; format = DXGI_FORMAT_R8G8_UNORM;          break;
            case PIXEL_FORMAT_RGBA8:    pixel_size = 4; format = DXGI_FORMAT_R8G8B8A8_UNORM;      break;
            case PIXEL_FORMAT_SRGB8_A8: pixel_size = 4; format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
            INVALID_DEFAULT_CASE;
        }

        D3D11_TEXTURE2D_DESC desc = {
            .Width      = params->w,
            .Height     = params->h,
            .MipLevels  = 1,
            .ArraySize  = 1,
            .Format     = format,
            .SampleDesc = { 1, 0 },
            .Usage      = D3D11_USAGE_IMMUTABLE,
            .BindFlags  = D3D11_BIND_SHADER_RESOURCE,
        };

        uint32_t pitch = params->pitch;

        if (!pitch)  
            pitch = params->w*pixel_size;

        if (params->flags & TEXTURE_FLAG_CUBEMAP)
        {
            desc.ArraySize = 6;
            desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

            D3D11_SUBRESOURCE_DATA data[6];
            for (size_t i = 0; i < 6; i++)
            {
                data[i] = (D3D11_SUBRESOURCE_DATA){
                    .pSysMem     = params->faces[i],
                    .SysMemPitch = pitch,
                };
            }

            ID3D11Device_CreateTexture2D(d3d.device, &desc, &data[0], &resource->tex[0]);

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
                .Format                = desc.Format,
                .ViewDimension         = D3D11_SRV_DIMENSION_TEXTURECUBE,
                .TextureCube.MipLevels = desc.MipLevels,
            };

            ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)resource->tex[0], &srv_desc, &resource->srv);
        }
        else
        {
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.MipLevels = 0;
            desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

            ID3D11Device_CreateTexture2D(d3d.device, &desc, NULL /*&data*/, &resource->tex[0]);
            ID3D11DeviceContext_UpdateSubresource(d3d.context, (ID3D11Resource *)resource->tex[0], 0, NULL, params->pixels, pitch, params->h*pitch);

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
                .Format              = desc.Format,
                .ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D,
                .Texture2D.MipLevels = UINT_MAX,
            };

            ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)resource->tex[0], &srv_desc, &resource->srv);
            ID3D11DeviceContext_GenerateMips(d3d.context, resource->srv);
        }
        result = bd_get_handle(&d3d_textures, resource);
    }

    return result;
}

void destroy_texture(resource_handle_t handle)
{
    IGNORED(handle);
    FATAL_ERROR("TODO: Implement");
}

resource_handle_t upload_model(const upload_model_t *params)
{
    resource_handle_t result = { 0 };

    if (NEVER(params->vertex_format >= VERTEX_FORMAT_COUNT))
        return result;

    d3d_model_t *resource = bd_add(&d3d_models);
    if (resource)
    {
        ASSERT(resource->vbuffer == NULL);
        ASSERT(resource->ibuffer == NULL);

        // vbuffer
        {
            switch (params->topology)
            {
                case R_PRIMITIVE_TOPOLOGY_TRIANGELIST:  resource->primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  break;
                case R_PRIMITIVE_TOPOLOGY_TRIANGESTRIP: resource->primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
                case R_PRIMITIVE_TOPOLOGY_LINELIST:     resource->primitive_topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;      break;
                case R_PRIMITIVE_TOPOLOGY_LINESTRIP:    resource->primitive_topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;     break;
                case R_PRIMITIVE_TOPOLOGY_POINTLIST:    resource->primitive_topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;     break;
                INVALID_DEFAULT_CASE;
            }

            resource->vertex_format      = params->vertex_format;
            resource->vcount             = params->vertex_count;

            UINT stride = vertex_format_size[resource->vertex_format];

            D3D11_BUFFER_DESC desc = {
                .ByteWidth = params->vertex_count*stride,
                .Usage     = D3D11_USAGE_IMMUTABLE,
                .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            };

            D3D11_SUBRESOURCE_DATA initial = { .pSysMem = params->vertices };
            ID3D11Device_CreateBuffer(d3d.device, &desc, &initial, &resource->vbuffer);
        }

        // ibuffer
        if (params->index_count > 0)
        {
            resource->icount = params->index_count;

            D3D11_BUFFER_DESC desc = {
                .ByteWidth = params->index_count*sizeof(uint16_t),
                .Usage     = D3D11_USAGE_IMMUTABLE,
                .BindFlags = D3D11_BIND_INDEX_BUFFER,
            };

            D3D11_SUBRESOURCE_DATA initial = { .pSysMem = params->indices };
            ID3D11Device_CreateBuffer(d3d.device, &desc, &initial, &resource->ibuffer);
        }

        result = bd_get_handle(&d3d_models, resource);
    }

    return result;
}

void destroy_model(resource_handle_t model)
{
    IGNORED(model);
    FATAL_ERROR("TODO: Implement");
}

ID3DBlob *compile_shader(string_t hlsl_file, string_t hlsl, const char *entry_point, const char *kind)
{
    UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR|D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined(DEBUG_RENDERER)
    flags |= D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* error;

    ID3DBlob* blob = NULL;
    HRESULT hr = D3DCompile(hlsl.data, hlsl.count, string_null_terminate(temp, hlsl_file), NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, kind, flags, 0, &blob, &error);
    if (FAILED(hr))
    {
        const char* message = ID3D10Blob_GetBufferPointer(error);
        OutputDebugStringA(message);
        FATAL_ERROR("Failed to compile shader. (%s, entry point '%s'):\n\n%s", kind, entry_point, message);
    }

    return blob;
}

ID3D11PixelShader *compile_ps(string_t hlsl_file, string_t hlsl, const char *entry_point)
{
    ID3DBlob* blob = compile_shader(hlsl_file, hlsl, entry_point, "ps_5_0");

    if (!blob)
        return NULL;

    ID3D11PixelShader *result;
    ID3D11Device_CreatePixelShader(d3d.device, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), NULL, &result);

    ID3D10Blob_Release(blob);

    return result;
}

ID3D11VertexShader *compile_vs(string_t hlsl_file, string_t hlsl, const char *entry_point)
{
    ID3DBlob* blob = compile_shader(hlsl_file, hlsl, entry_point, "vs_5_0");

    if (!blob)
        return NULL;

    ID3D11VertexShader *result;
    ID3D11Device_CreateVertexShader(d3d.device, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), NULL, &result);

    ID3D10Blob_Release(blob);

    return result;
}

void update_buffer(ID3D11Buffer *buffer, const void *data, size_t size)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11DeviceContext_Map(d3d.context, (ID3D11Resource *)buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, data, size);
    ID3D11DeviceContext_Unmap(d3d.context, (ID3D11Resource *)buffer, 0);
}

void set_model_buffers(d3d_model_t *model)
{
    ID3D11DeviceContext_IASetInputLayout(d3d.context, d3d.layouts[model->vertex_format]);
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d.context, model->primitive_topology);
    UINT stride = vertex_format_size[model->vertex_format];
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(d3d.context, 0, 1, &model->vbuffer, &stride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d.context, model->ibuffer, DXGI_FORMAT_R16_UINT, 0);
}

render_api_i *d3d11_get_api()
{
    static render_api_i api = {
        .get_resolution  = get_resolution,

        .upload_texture  = upload_texture,
        .destroy_texture = destroy_texture,

        .upload_model    = upload_model,
        .destroy_model   = destroy_model,
    };
    return &api;
}
