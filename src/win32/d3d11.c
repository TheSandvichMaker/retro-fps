#define COBJMACROS
#define NOMINMAX

#pragma warning(push, 0)

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_2.h>
#include <d3dcompiler.h>
#include <math.h>
#include <stdio.h>

#pragma warning(pop)

#include "d3d11.h"
#include "d3d11_internal.h"
#include "d3d11_data.h"

#ifndef NDEBUG
#define DEBUG_RENDERER
#endif

#define SUN_SHADOWMAP_RESOLUTION (1024)

extern _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

bulk_t d3d_models   = INIT_BULK_DATA_EX(d3d_model_t,   BULK_FLAGS_CONCURRENT);
bulk_t d3d_textures = INIT_BULK_DATA_EX(d3d_texture_t, BULK_FLAGS_CONCURRENT);

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
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
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
        D3D11_INPUT_ELEMENT_DESC layout_pos[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(vertex_pos_t, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D11_INPUT_ELEMENT_DESC layout_immediate[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(vertex_immediate_t, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, offsetof(vertex_immediate_t, tex), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32_UINT,            0, offsetof(vertex_immediate_t, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D11_INPUT_ELEMENT_DESC layout_brush[] = {
            { "POSITION",          0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex_brush_t,     pos),          D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",          0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex_brush_t,     tex),          D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD_LIGHTMAP", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex_brush_t,     tex_lightmap), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",            0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex_brush_t,     normal),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        string_t hlsl_file = strlit("gamedata/shaders/input_layout_nonsense.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        ID3DBlob *vs_pos       = compile_shader(hlsl_file, hlsl, "pos", "vs_5_0");
        ID3DBlob *vs_immediate = compile_shader(hlsl_file, hlsl, "immediate", "vs_5_0");
        ID3DBlob *vs_brush     = compile_shader(hlsl_file, hlsl, "brush", "vs_5_0");

        ID3D11Device_CreateInputLayout(d3d.device, layout_pos, ARRAY_COUNT(layout_pos), 
                                       ID3D10Blob_GetBufferPointer(vs_pos), ID3D10Blob_GetBufferSize(vs_pos), 
                                       &d3d.layouts[VERTEX_FORMAT_POS]);

        ID3D11Device_CreateInputLayout(d3d.device, layout_immediate, ARRAY_COUNT(layout_immediate), 
                                       ID3D10Blob_GetBufferPointer(vs_immediate), ID3D10Blob_GetBufferSize(vs_immediate), 
                                       &d3d.layouts[VERTEX_FORMAT_IMMEDIATE]);

        ID3D11Device_CreateInputLayout(d3d.device, layout_brush, ARRAY_COUNT(layout_brush), 
                                       ID3D10Blob_GetBufferPointer(vs_brush), ID3D10Blob_GetBufferSize(vs_brush),
                                       &d3d.layouts[VERTEX_FORMAT_BRUSH]);

        ID3D10Blob_Release(vs_pos);
        ID3D10Blob_Release(vs_immediate);
        ID3D10Blob_Release(vs_brush);
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
        string_t hlsl_file = strlit("gamedata/shaders/postprocess.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.postprocess_vs  = compile_vs(hlsl_file, hlsl, "postprocess_vs");
        d3d.msaa_resolve_ps = compile_ps(hlsl_file, hlsl, "msaa_resolve_ps");
        d3d.hdr_resolve_ps  = compile_ps(hlsl_file, hlsl, "hdr_resolve_ps");
    }

    m_scoped(temp)
    {
        string_t hlsl_file = strlit("gamedata/shaders/shadowmap.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.shadowmap_vs = compile_vs(hlsl_file, hlsl, "shadowmap_vs");
    }

    // create constant buffers

    {
        D3D11_BUFFER_DESC desc = {
            .ByteWidth      = (UINT)align_forward(sizeof(d3d_cbuffer_t), 16),
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

    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_BORDER,
            .AddressV = D3D11_TEXTURE_ADDRESS_BORDER,
            .AddressW = D3D11_TEXTURE_ADDRESS_BORDER,
            .MaxLOD   = FLT_MAX,
        };
        ID3D11Device_CreateSamplerState(d3d.device, &desc, &d3d.samplers[D3D_SAMPLER_FOG]);
    }

    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
            .AddressU       = D3D11_TEXTURE_ADDRESS_BORDER,
            .AddressV       = D3D11_TEXTURE_ADDRESS_BORDER,
            .AddressW       = D3D11_TEXTURE_ADDRESS_BORDER,
            .ComparisonFunc = D3D11_COMPARISON_LESS,
            .BorderColor    = { 1, 1, 1, 1 },
            .MaxLOD         = FLT_MAX,
        };
        ID3D11Device_CreateSamplerState(d3d.device, &desc, &d3d.samplers[D3D_SAMPLER_SHADOWMAP]);
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

    // additive blend state

    {
        D3D11_BLEND_DESC desc =
        {
            .RenderTarget[0] = {
                .BlendEnable           = TRUE,
                .SrcBlend              = D3D11_BLEND_SRC_ALPHA,
                .DestBlend             = D3D11_BLEND_ONE,
                .BlendOp               = D3D11_BLEND_OP_ADD,
                .SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA,
                .DestBlendAlpha        = D3D11_BLEND_ONE,
                .BlendOpAlpha          = D3D11_BLEND_OP_ADD,
                .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
            },
        };
        ID3D11Device_CreateBlendState(d3d.device, &desc, &d3d.bs_additive);
    }

    // create rasterizer state

    {
        D3D11_RASTERIZER_DESC desc = {
            .FillMode              = D3D11_FILL_SOLID,
            .CullMode              = D3D11_CULL_BACK,
            .ScissorEnable         = TRUE,
            .AntialiasedLineEnable = TRUE,
        };
        ID3D11Device_CreateRasterizerState(d3d.device, &desc, &d3d.rs);
    }

    // cull front rasterizer

    {
        D3D11_RASTERIZER_DESC desc = {
            .FillMode              = D3D11_FILL_SOLID,
            .CullMode              = D3D11_CULL_FRONT,
            .ScissorEnable         = TRUE,
            .AntialiasedLineEnable = TRUE,
        };
        ID3D11Device_CreateRasterizerState(d3d.device, &desc, &d3d.rs_cull_front);
    }

    // create no-cull rasterizer state

    {
        D3D11_RASTERIZER_DESC desc = {
            .FillMode              = D3D11_FILL_SOLID,
            .CullMode              = D3D11_CULL_NONE,
            .ScissorEnable         = TRUE,
            .AntialiasedLineEnable = TRUE,
        };
        ID3D11Device_CreateRasterizerState(d3d.device, &desc, &d3d.rs_no_cull);
    }

    // create depth & stencil

    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable      = TRUE,
            .DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc        = D3D11_COMPARISON_GREATER,
        };
        ID3D11Device_CreateDepthStencilState(d3d.device, &desc, &d3d.dss);
    }

    // create no-depth test depth & stencil

    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable      = FALSE,
            .DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc        = D3D11_COMPARISON_GREATER,
        };
        ID3D11Device_CreateDepthStencilState(d3d.device, &desc, &d3d.dss_no_depth);
    }

    // don't write depth

    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable      = TRUE,
            .DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO,
            .DepthFunc        = D3D11_COMPARISON_GREATER,
        };
        ID3D11Device_CreateDepthStencilState(d3d.device, &desc, &d3d.dss_dont_write_depth);
    }

    // shadowmap depth state

    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable      = TRUE,
            .DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc        = D3D11_COMPARISON_LESS,
        };
        ID3D11Device_CreateDepthStencilState(d3d.device, &desc, &d3d.dss_less);
    }

    // white texture

    {
        uint32_t pixel = 0xFFFFFFFF;

        resource_handle_t handle = upload_texture(&(upload_texture_t){
            .desc = {
                .format = PIXEL_FORMAT_RGBA8,
                .w      = 1,
                .h      = 1,
            },
            .data = {
                .pixels = &pixel,
            },
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
            .desc = {
                .format = PIXEL_FORMAT_RGBA8,
                .w      = w,
                .h      = h,
            },
            .data = {
                .pixels = pixels,
            },
        });
        d3d.missing_texture = bd_get(&d3d_textures, handle);

        resource_handle_t cubemap_handle = upload_texture(&(upload_texture_t){
            .desc = {
                .format = PIXEL_FORMAT_RGBA8,
                .w      = w,
                .h      = h,
                .flags  = TEXTURE_FLAG_CUBEMAP,
            },
            .data = {
                .faces = {
                    pixels,
                    pixels,
                    pixels,
                    pixels,
                    pixels,
                    pixels,
                },
            },
        });
        d3d.missing_texture_cubemap = bd_get(&d3d_textures, cubemap_handle);
    }

    // blue noise texture
    m_scoped(temp)
    {
        image_t t0 = load_image_from_disk(temp, strlit("gamedata/textures/noise/LDR_LLL1_0.png"), 1);
        image_t t1 = load_image_from_disk(temp, strlit("gamedata/textures/noise/LDR_LLL1_7.png"), 1);
        image_t t2 = load_image_from_disk(temp, strlit("gamedata/textures/noise/LDR_LLL1_15.png"), 1);
        image_t t3 = load_image_from_disk(temp, strlit("gamedata/textures/noise/LDR_LLL1_23.png"), 1);

        uint32_t *pixels = m_alloc_array(temp, t0.w*t0.h, uint32_t);

        unsigned char *t0p = t0.pixels;
        unsigned char *t1p = t1.pixels;
        unsigned char *t2p = t2.pixels;
        unsigned char *t3p = t3.pixels;

        for (size_t i = 0; i < t0.w*t0.h; i++)
        {
            pixels[i] = ((t0p[i] <<  0) | 
                         (t1p[i] <<  8) |
                         (t2p[i] << 16) |
                         (t3p[i] << 24));
        }

        resource_handle_t handle = render->upload_texture(&(upload_texture_t){
            .desc = {
                .format = PIXEL_FORMAT_RGBA8,
                .w      = t0.w,
                .h      = t0.h,
            },
            .data = {
                .pixels = pixels,
            },
        });
        d3d.blue_noise = bd_get(&d3d_textures, handle);
    }

    // create immediate rendering buffers

    {
        size_t stride = sizeof(uint32_t);
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
        size_t stride = vertex_format_size[VERTEX_FORMAT_IMMEDIATE];
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

    // sun shadow map
    {
        d3d.sun_shadowmap = d3d_create_rendertarget(&(d3d_create_rendertarget_t){
            .w = SUN_SHADOWMAP_RESOLUTION,
            .h = SUN_SHADOWMAP_RESOLUTION,
            // TODO: Specify depth format
            .create_depth = true,
            .create_srv   = true,
        });
    }

    // queries
    {
        d3d.query_collect_frame = -1;

        for (int query_frame = 0; query_frame < D3D_QUERY_FRAME_COUNT; query_frame++)
        {
            d3d_queries_t *queries = &d3d.queries[query_frame];

            ID3D11Device_CreateQuery(d3d.device, &(D3D11_QUERY_DESC){ D3D11_QUERY_TIMESTAMP_DISJOINT }, &queries->disjoint);

            for (int ts = 0; ts < RENDER_TS_COUNT; ts++)
            {
                ID3D11Query **q = &queries->ts[ts];
                if (FAILED(ID3D11Device_CreateQuery(d3d.device, &(D3D11_QUERY_DESC){ D3D11_QUERY_TIMESTAMP }, q)))
                {
                    FATAL_ERROR("What on earth");
                }
            }
        }
    }

    return 0;
}

void render_model(const render_pass_t *pass)
{
    ASSERT(pass->index_format == DXGI_FORMAT_R16_UINT ||
           pass->index_format == DXGI_FORMAT_R32_UINT);

    // set vertex buffer and friends
    set_model_buffers(pass->model, pass->index_format);

    // set vertex shader
    ID3D11DeviceContext_VSSetConstantBuffers(d3d.context, 0, pass->cbuffer_count, pass->cbuffers);
    ID3D11DeviceContext_VSSetShader(d3d.context, pass->vs, NULL, 0);

    // set rasterizer state
    ID3D11DeviceContext_RSSetViewports(d3d.context, 1, &pass->viewport);

    ID3D11RasterizerState *rs = d3d.rs_no_cull;

    switch (pass->cull)
    {
        case D3D_CULL_BACK:  rs = d3d.rs; break;
        case D3D_CULL_FRONT: rs = d3d.rs_cull_front; break;
    }

    ID3D11DeviceContext_RSSetState(d3d.context, rs);

	D3D11_RECT scissor_rect = pass->scissor_rect;
	if (scissor_rect.bottom < scissor_rect.top) scissor_rect.bottom = scissor_rect.top;
	if (scissor_rect.right < scissor_rect.left) scissor_rect.right = scissor_rect.left;

    if (pass->scissor)
        ID3D11DeviceContext_RSSetScissorRects(d3d.context, 1, &scissor_rect);
    else
        ID3D11DeviceContext_RSSetScissorRects(d3d.context, 1, (&(D3D11_RECT){ 0, 0, d3d.current_width, d3d.current_height }));

    // set pixel shader
    ID3D11DeviceContext_PSSetSamplers(d3d.context, 0, D3D_SAMPLER_COUNT, d3d.samplers);
    ID3D11DeviceContext_PSSetShaderResources(d3d.context, 0, pass->srv_count, pass->srvs);
    ID3D11DeviceContext_PSSetShader(d3d.context, pass->ps, NULL, 0);

    // set output merger state

    if (pass->blend_mode == R_BLEND_PREMUL_ALPHA)
    {
        ID3D11DepthStencilState *dss = d3d.dss;

        switch (pass->depth)
        {
            case D3D_DEPTH_TEST_GREATER: dss = d3d.dss; break;
            case D3D_DEPTH_TEST_LESS:    dss = d3d.dss_less; break;
            case D3D_DEPTH_TEST_NONE:    dss = d3d.dss_no_depth; break;
        }

        ID3D11DeviceContext_OMSetBlendState(d3d.context, d3d.bs, NULL, ~0U);
        ID3D11DeviceContext_OMSetDepthStencilState(d3d.context, dss, 0);
    }
    else
    {
        ASSERT(pass->blend_mode == R_BLEND_ADDITIVE);
        ID3D11DeviceContext_OMSetBlendState(d3d.context, d3d.bs_additive, NULL, ~0U);
        ID3D11DeviceContext_OMSetDepthStencilState(d3d.context, d3d.dss_dont_write_depth, 0);
    }

    ID3D11DeviceContext_OMSetRenderTargets(d3d.context, 1, &pass->render_target, pass->depth_stencil);

    // draw 
    if (pass->model->icount > 0)
        ID3D11DeviceContext_DrawIndexed(d3d.context, pass->model->icount, pass->ioffset, 0);
    else
        ID3D11DeviceContext_Draw(d3d.context, pass->model->vcount, pass->voffset);
}

static d3d_texture_t *d3d_get_texture_or(resource_handle_t handle, d3d_texture_t *fallback)
{
	d3d_texture_t *texture = bd_get(&d3d_textures, handle);

	if (!texture || texture->state != D3D_TEXTURE_STATE_LOADED)  
		texture = fallback;

	return texture;
}

void d3d_do_post_pass(const d3d_post_pass_t *pass)
{
    // set output merger state
    ID3D11DeviceContext_OMSetBlendState(d3d.context, d3d.bs, NULL, ~0U);
    ID3D11DeviceContext_OMSetDepthStencilState(d3d.context, d3d.dss_no_depth, 0);
    ID3D11DeviceContext_OMSetRenderTargets(d3d.context, 1, &pass->render_target, NULL);

    ID3D11DeviceContext_IASetInputLayout(d3d.context, NULL);

    // set vertex shader
    ID3D11DeviceContext_VSSetConstantBuffers(d3d.context, 0, 1, &d3d.ubuffer);
    ID3D11DeviceContext_VSSetShader(d3d.context, d3d.postprocess_vs, NULL, 0);

    // set rasterizer state
    ID3D11DeviceContext_RSSetViewports(d3d.context, 1, &pass->viewport);
    ID3D11DeviceContext_RSSetState(d3d.context, d3d.rs_no_cull);
    ID3D11DeviceContext_RSSetScissorRects(d3d.context, 1, (&(D3D11_RECT){ 0, 0, d3d.current_width, d3d.current_height }));

    // set pixel shader
    ID3D11DeviceContext_PSSetConstantBuffers(d3d.context, 0, 1, &d3d.ubuffer);
    ID3D11DeviceContext_PSSetSamplers(d3d.context, 0, D3D_SAMPLER_COUNT, d3d.samplers);
    ID3D11DeviceContext_PSSetShaderResources(d3d.context, 0, pass->srv_count, pass->srvs);
    ID3D11DeviceContext_PSSetShader(d3d.context, pass->ps, NULL, 0);

    ID3D11DeviceContext_Draw(d3d.context, 3, 0);

    ID3D11ShaderResourceView *insane_people_made_this_api[] = { NULL, NULL, NULL, NULL, NULL };
    ID3D11DeviceContext_PSSetShaderResources(d3d.context, 0, 5, insane_people_made_this_api);
}

d3d_rendertarget_t d3d_create_rendertarget(const d3d_create_rendertarget_t *params)
{
    d3d_rendertarget_t result = {0};

    if (params->create_color)
    {
        D3D11_TEXTURE2D_DESC rt_desc = {
            .Width      = params->w,
            .Height     = params->h,
            .MipLevels  = params->allow_color_mipmaps ? 0 : 1,
            .ArraySize  = 1,
            .Format     = params->format,
            .SampleDesc = { MAX(1, params->msaa_samples), 0 },
            .Usage      = D3D11_USAGE_DEFAULT,
            .BindFlags  = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,
        };

        if (params->allow_color_mipmaps)
        {
            rt_desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }

        ID3D11Device_CreateTexture2D(d3d.device, &rt_desc, NULL, &result.color_tex);
        ID3D11Device_CreateRenderTargetView(d3d.device, (ID3D11Resource *)result.color_tex, NULL, &result.color_rtv);
        if (params->create_srv)
        {
            ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)result.color_tex, NULL, &result.color_srv);
        }
    }

    if (params->create_depth)
    {
        D3D11_TEXTURE2D_DESC depth_desc = {
            .Width      = params->w,
            .Height     = params->h,
            .MipLevels  = params->allow_depth_mipmaps ? 0 : 1,
            .ArraySize  = 1,
            .Format     = DXGI_FORMAT_R32_TYPELESS, // or use DXGI_FORMAT_D32_FLOAT_S8X24_UINT if you need stencil
            .SampleDesc = { MAX(1, params->msaa_samples), 0 },
            .Usage      = D3D11_USAGE_DEFAULT,
            .BindFlags  = D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE,
        };

        if (params->allow_depth_mipmaps)
        {
            depth_desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }

        ID3D11Device_CreateTexture2D(d3d.device, &depth_desc, NULL, &result.depth_tex);
                                                                                                                   
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
            .Format = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = params->msaa_samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D,
        };
        ID3D11Device_CreateDepthStencilView(d3d.device, (ID3D11Resource*)result.depth_tex, &dsv_desc, &result.depth_dsv);
                                                                                                                   
        D3D11_SHADER_RESOURCE_VIEW_DESC depth_srv_desc = {
            .Format = DXGI_FORMAT_R32_FLOAT,
            .ViewDimension = params->msaa_samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D,
        };
        if (params->msaa_samples <= 1)
        {
            depth_srv_desc.Texture2D.MipLevels = 1;
        }
        ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource*)result.depth_tex, &depth_srv_desc, &result.depth_srv);
    }

    return result;
}

void d3d_release_rendertarget(d3d_rendertarget_t *rt)
{
    if (rt->color_tex) { ID3D11Texture2D_Release         (rt->color_tex); rt->color_tex = NULL; }
    if (rt->color_rtv) { ID3D11RenderTargetView_Release  (rt->color_rtv); rt->color_rtv = NULL; }
    if (rt->color_srv) { ID3D11ShaderResourceView_Release(rt->color_srv); rt->color_srv = NULL; }

    if (rt->depth_tex) { ID3D11Texture2D_Release         (rt->depth_tex); rt->depth_tex = NULL; }
    if (rt->depth_dsv) { ID3D11RenderTargetView_Release  (rt->depth_dsv); rt->depth_dsv = NULL; }
    if (rt->depth_srv) { ID3D11ShaderResourceView_Release(rt->depth_srv); rt->depth_srv = NULL; }
}

void d3d_ensure_swap_chain_size(int width, int height)
{
    if (d3d.backbuffer.color_rtv == NULL || width != d3d.current_width || height != d3d.current_height)
    {
        if (d3d.backbuffer.color_rtv)
        {
            // release old swap chain buffers
            ID3D11DeviceContext_ClearState(d3d.context);
            d3d_release_rendertarget(&d3d.scene_target);
            d3d_release_rendertarget(&d3d.post_target);
            d3d_release_rendertarget(&d3d.backbuffer);
        }

        // resize to new size for non-zero size
        if (width != 0 && height != 0)
        {
            IDXGISwapChain1_ResizeBuffers(d3d.swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);

            IDXGISwapChain1_GetBuffer(d3d.swap_chain, 0, &IID_ID3D11Texture2D, &d3d.backbuffer.color_tex);
			D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
				.Texture2D = {
					.MipSlice = 0,
				},
			};
            ID3D11Device_CreateRenderTargetView(d3d.device, (ID3D11Resource *)d3d.backbuffer.color_tex, &rtv_desc, &d3d.backbuffer.color_rtv);

            d3d.scene_target = d3d_create_rendertarget(&(d3d_create_rendertarget_t) {
                .w            = width,
                .h            = height,
                .format       = DXGI_FORMAT_R11G11B10_FLOAT,
                .msaa_samples = 8,
                .create_color = true,
                .create_depth = true,
                .create_srv   = true,
            });

            d3d.post_target = d3d_create_rendertarget(&(d3d_create_rendertarget_t) {
                .w                   = width,
                .h                   = height,
                .format              = DXGI_FORMAT_R11G11B10_FLOAT,
                .msaa_samples        = 1,
                .create_color        = true,
                .create_srv          = true,
                .allow_color_mipmaps = true,
            });
        }

        d3d.current_width = width;
        d3d.current_height = height;
    }
}

void d3d11_draw_list(r_list_t *list, int width, int height)
{
    uint32_t frame_index = d3d.frame_index;

    debug_print("Querying timestamps for query frame %d\n", d3d.query_frame);

    d3d_queries_t *queries = &d3d.queries[d3d.query_frame];
    ID3D11DeviceContext_Begin(d3d.context, (ID3D11Asynchronous *)queries->disjoint);
    d3d_timestamp(RENDER_TS_BEGIN_FRAME);

	AcquireSRWLockExclusive(&d3d.context_lock);

    d3d_ensure_swap_chain_size(width, height);

    if (d3d.backbuffer.color_rtv)
    {
        //
        // Render Sun Shadows
        //

        ID3D11DeviceContext_ClearDepthStencilView(d3d.context, d3d.sun_shadowmap.depth_dsv, D3D11_CLEAR_DEPTH, 0.0f, 0);

        d3d_timestamp(RENDER_TS_CLEAR_SHADOWMAP);

        // unfortunate
        r_view_t *player_view = &list->views[list->view_stack[2]];
        (void)player_view;
        v3_t camera_p = make_v3(0, 0, 0); // player_view->camera_p;

        D3D11_VIEWPORT sun_viewport =
        {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width    = SUN_SHADOWMAP_RESOLUTION,
            .Height   = SUN_SHADOWMAP_RESOLUTION,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };

        static float timer = 0.0f;
        v3_t sun_direction = normalize(make_v3(0.25f, 0.75f, 1));

        timer += 1.0f / 60.0f;

        m4x4_t sun_view   = make_view_matrix(add(camera_p, mul(-256.0f, sun_direction)), negate(sun_direction), make_v3(0, 0, 1));
        m4x4_t sun_ortho  = make_orthographic_matrix(2048, 2048, 512);
        m4x4_t sun_matrix = mul(sun_ortho, sun_view);

        for (char *at = list->command_list_base; at < list->command_list_at;)
        {
            r_command_base_t *base = (r_command_base_t *)at;

            switch (base->kind)
            {
                case R_COMMAND_MODEL:
                {
                    r_command_model_t *command = (r_command_model_t *)base;
                    at = align_address(at + sizeof(*command), RENDER_COMMAND_ALIGN);

                    d3d_model_t *model = bd_get(&d3d_models, command->model);
                    if (model)
                    {
                        d3d_cbuffer_t cbuffer = {
                            .sun_matrix   = sun_matrix,
                            .light_direction = sun_direction,
                            .model_matrix = command->transform,
                            .frame_index  = frame_index,
                        };

                        update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

                        render_model(&(render_pass_t) {
                            .render_target = NULL,
                            .depth_stencil = d3d.sun_shadowmap.depth_dsv,

                            .model = model,

                            .index_format = DXGI_FORMAT_R16_UINT,

                            .vs = d3d.shadowmap_vs,
                            .ps = NULL,

                            .cbuffer_count = 1,
                            .cbuffers      = (ID3D11Buffer *[]) { d3d.ubuffer },

                            .depth         = D3D_DEPTH_TEST_GREATER,
                            .cull          = D3D_CULL_BACK,
                            .viewport      = sun_viewport,
                            .scissor       = true,
                            .scissor_rect  = (D3D11_RECT){ 0, 0, SUN_SHADOWMAP_RESOLUTION, SUN_SHADOWMAP_RESOLUTION },
                        });
                    }
                } break;

                case R_COMMAND_IMMEDIATE:
                {
                    r_command_immediate_t *command = (r_command_immediate_t *)base;
                    at = align_address(at + sizeof(*command), RENDER_COMMAND_ALIGN);
                } break;

                case R_COMMAND_END_SCENE_PASS:
                {
                    goto done_with_sun_shadows;
                } break;

                INVALID_DEFAULT_CASE;
            }
        }
done_with_sun_shadows:

        d3d_timestamp(RENDER_TS_RENDER_SHADOWMAP);

        //
        // Actually Render Scene
        //

        D3D11_VIEWPORT viewport =
        {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width    = (FLOAT)width,
            .Height   = (FLOAT)height,
            .MinDepth = 0,
            .MaxDepth = 1,
        };

        float clear_color[4] = { 0, 0, 0, 1 };
        ID3D11DeviceContext_ClearRenderTargetView(d3d.context, d3d.scene_target.color_rtv, clear_color);
        ID3D11DeviceContext_ClearDepthStencilView(d3d.context, d3d.scene_target.depth_dsv, D3D11_CLEAR_DEPTH, 0.0f, 0);

        d3d_timestamp(RENDER_TS_CLEAR_MAIN);

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
                .view_matrix = mvp,
                .frame_index = frame_index,
                .sun_color   = view->sun_color,
            };

            update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

            d3d_model_t   *model   = d3d.skybox_model;

            d3d_texture_t *texture = d3d_get_texture_or(view->skybox, d3d.missing_texture_cubemap);

            render_model(&(render_pass_t) {
                .render_target = d3d.scene_target.color_rtv,
                .depth_stencil = d3d.scene_target.depth_dsv,

                .model = model,

                .vs = d3d.skybox_vs,
                .ps = d3d.skybox_ps,

                .index_format = DXGI_FORMAT_R16_UINT,

                .cbuffer_count = 1,
                .cbuffers      = (ID3D11Buffer *[]) { d3d.ubuffer },
                .srv_count     = 1,
                .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv, },

                .depth         = D3D_DEPTH_TEST_NONE,
                .cull          = D3D_CULL_NONE,
                .sample_linear = true,
                .viewport      = viewport,
            });
        }

        d3d_timestamp(RENDER_TS_RENDER_SKYBOX);

        // update immediate index/vertex buffer
        update_buffer(d3d.immediate_ibuffer, list->immediate_indices,  sizeof(list->immediate_indices[0])*list->immediate_icount);
        update_buffer(d3d.immediate_vbuffer, list->immediate_vertices, sizeof(list->immediate_vertices[0])*list->immediate_vcount);

        d3d_timestamp(RENDER_TS_UPLOAD_IMMEDIATE_DATA);

        bool resolved_scene = false;

        d3d_rendertarget_t *current_render_target = &d3d.scene_target;

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
                        d3d_texture_t *texture  = d3d_get_texture_or(command->texture, d3d.missing_texture);
                        d3d_texture_t *lightmap = d3d_get_texture_or(command->lightmap, d3d.white_texture);

                        m4x4_t camera_projection = m4x4_identity;
                        camera_projection = mul(camera_projection, view->projection);
                        camera_projection = mul(camera_projection, view->camera);

                        d3d_cbuffer_t cbuffer = {
                            .view_matrix     = view->camera,
                            .proj_matrix     = view->projection,
                            .sun_matrix      = sun_matrix,
                            .light_direction = sun_direction,
                            .model_matrix    = command->transform,
                            .frame_index     = frame_index,
                            .sun_color       = view->sun_color,
                            .sun_direction   = sun_direction,
                            .fog_density     = view->fog_density,
                            .fog_absorption  = view->fog_absorption,
                            .fog_scattering  = view->fog_scattering,
                            .fog_phase_k     = view->fog_phase_k,
                        };

                        update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

                        render_model(&(render_pass_t) {
                            .render_target = current_render_target->color_rtv,
                            .depth_stencil = current_render_target->depth_dsv,

                            .model = model,

                            .index_format = DXGI_FORMAT_R16_UINT,

                            .vs = d3d.world_vs,
                            .ps = d3d.world_ps,

                            .cbuffer_count = 1,
                            .cbuffers      = (ID3D11Buffer *[]) { d3d.ubuffer },
                            .srv_count     = 3,
                            .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv, lightmap->srv, d3d.sun_shadowmap.depth_srv, },

                            .depth         = D3D_DEPTH_TEST_GREATER,
                            .cull          = D3D_CULL_BACK,
                            .sample_linear = false,
                            .viewport      = viewport,
                        });
                    }
                } break;

                case R_COMMAND_IMMEDIATE:
                {
                    r_command_immediate_t *command = (r_command_immediate_t *)base;
                    at = align_address(at + sizeof(*command), RENDER_COMMAND_ALIGN);

                    r_immediate_draw_t *draw_call = &command->draw_call;

                    d3d_cbuffer_t cbuffer = {
                        .view_matrix     = view->camera,
                        .proj_matrix     = view->projection,
                        .sun_matrix      = sun_matrix,
                        .light_direction = sun_direction,
                        .model_matrix    = m4x4_identity,
                        .depth_bias      = draw_call->params.depth_bias,
                        .sun_direction   = sun_direction,
                        .fog_density     = view->fog_density,
                        .fog_absorption  = view->fog_absorption,
                        .fog_scattering  = view->fog_scattering,
                        .fog_phase_k     = view->fog_phase_k,
                    };

                    update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

                    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                    switch (draw_call->params.topology)
                    {
                        case R_PRIMITIVE_TOPOLOGY_TRIANGELIST: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
                        case R_PRIMITIVE_TOPOLOGY_LINELIST:    topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
                        case R_PRIMITIVE_TOPOLOGY_LINESTRIP:   topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
                        case R_PRIMITIVE_TOPOLOGY_POINTLIST:   topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
                        INVALID_DEFAULT_CASE;
                    }

                    // TODO: index/vertex count is specified here, but the offset is in the pass. feels a bit janky.
                    d3d_model_t immediate_model = {
                        .vertex_format      = VERTEX_FORMAT_IMMEDIATE,
                        .primitive_topology = topology,
                        .icount             = draw_call->icount,
                        .ibuffer            = d3d.immediate_ibuffer,
                        .vcount             = draw_call->vcount,
                        .vbuffer            = d3d.immediate_vbuffer,
                    };

                    d3d_texture_t *texture = d3d_get_texture_or(draw_call->params.texture, d3d.white_texture);

					rect2_t clip_rect = rect2_intersect(draw_call->params.clip_rect, view->clip_rect);

                    render_model(&(render_pass_t) {
                        .render_target = current_render_target->color_rtv,
                        .depth_stencil = current_render_target->depth_dsv,

                        .model = &immediate_model,

                        .vs = d3d.immediate_vs,
                        .ps = d3d.immediate_ps,

                        .blend_mode   = draw_call->params.blend_mode,
                        .index_format = DXGI_FORMAT_R32_UINT,

                        .ioffset = draw_call->ioffset,
                        .voffset = draw_call->voffset,

                        .cbuffer_count = 1,
                        .cbuffers      = (ID3D11Buffer *[]) { d3d.ubuffer },
                        .srv_count     = 1,
                        .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv },

                        .depth    = draw_call->params.depth_test ? D3D_DEPTH_TEST_GREATER : D3D_DEPTH_TEST_NONE,
                        .cull     = D3D_CULL_NONE,
                        .viewport = viewport,
                        .scissor  = true,
                        .scissor_rect = {
                            .left   = (LONG)clip_rect.min.x,
                            .right  = (LONG)clip_rect.max.x,
                            .bottom = height - (LONG)clip_rect.min.y,
                            .top    = height - (LONG)clip_rect.max.y,
                        },
                    });
                } break;

                case R_COMMAND_END_SCENE_PASS:
                {
                    at = align_address(at + sizeof(*base), RENDER_COMMAND_ALIGN);

                    if (ALWAYS(!resolved_scene))
                    {
                        d3d_timestamp(RENDER_TS_SCENE);

                        resolved_scene = true;

                        d3d_texture_t *fogmap = d3d_get_texture_or(view->fogmap, NULL);

                        ID3D11ShaderResourceView *fogmap_srv = fogmap ? fogmap->srv : NULL;

                        d3d_cbuffer_t cbuffer = {
                            .view_matrix  = view->camera,
                            .proj_matrix  = view->projection,
                            .model_matrix = m4x4_identity,
                            .sun_matrix   = sun_matrix,
                            .frame_index  = frame_index,
                            .fog_offset   = view->fog_offset,
                            .fog_dim      = view->fog_dim,
                            .sun_color    = view->sun_color,
                            .sun_direction = sun_direction,
                            .fog_density     = view->fog_density,
                            .fog_absorption  = view->fog_absorption,
                            .fog_scattering  = view->fog_scattering,
                            .fog_phase_k     = view->fog_phase_k,
                        };

                        update_buffer(d3d.ubuffer, &cbuffer, sizeof(cbuffer));

                        d3d_do_post_pass(&(d3d_post_pass_t){
                            .render_target = d3d.post_target.color_rtv,
                            .ps            = d3d.msaa_resolve_ps,
                            .srv_count     = 5,
                            .srvs          = (ID3D11ShaderResourceView *[]){ d3d.scene_target.color_srv, d3d.scene_target.depth_srv, d3d.blue_noise->srv, fogmap_srv, d3d.sun_shadowmap.depth_srv },
                            .viewport      = viewport,
                        });

                        d3d_timestamp(RENDER_TS_MSAA_RESOLVE);

                        // Generate mipmaps for bloom
                        ID3D11DeviceContext_GenerateMips(d3d.context, d3d.post_target.color_srv);

                        d3d_timestamp(RENDER_TS_BLOOM);

                        //
                        // Post processing, resolve HDR
                        //

                        d3d_do_post_pass(&(d3d_post_pass_t){
                            .render_target = d3d.backbuffer.color_rtv,
                            .ps            = d3d.hdr_resolve_ps,
                            .srv_count     = 3,
                            .srvs          = (ID3D11ShaderResourceView *[]){ d3d.post_target.color_srv, NULL, d3d.blue_noise->srv },
                            .viewport      = viewport,
                        });

                        d3d_timestamp(RENDER_TS_POST_PASS);

                        current_render_target = &d3d.backbuffer;
                    }
                } break;

                INVALID_DEFAULT_CASE;
            }
        }

        d3d_timestamp(RENDER_TS_UI);
    }

	ReleaseSRWLockExclusive(&d3d.context_lock);

    d3d.frame_index++;
}

DREAM_INLINE void d3d_collect_timestamp_data(void)
{
    if (d3d.query_collect_frame < 0)
    {
        d3d.query_collect_frame += 1;
        return;
    }

    d3d_queries_t *queries = &d3d.queries[d3d.query_collect_frame];

    while (ID3D11DeviceContext_GetData(d3d.context, (ID3D11Asynchronous *)queries->disjoint, NULL, 0, 0) == S_FALSE)
    {
        debug_print("Snoozin on da query...\n");
        Sleep(1);
    }

    debug_print("Collecting timestamps for collect frame %d\n", d3d.query_collect_frame);

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT ts_disjoint;
    if (ID3D11DeviceContext_GetData(d3d.context, (ID3D11Asynchronous *)queries->disjoint, &ts_disjoint, sizeof(ts_disjoint), 0) != S_OK)
    {
        debug_print("Couldn't retrieve timestamp disjoint query data\n");
        return;
    }

    if (ts_disjoint.Disjoint)
    {
        debug_print("Timestamp data is disjoint\n");
        return;
    }

    uint64_t timestamp_prev = 0;
    for (int ts = 0; ts < RENDER_TS_COUNT; ts++)
    {
        uint64_t timestamp = timestamp_prev;
        while (ID3D11DeviceContext_GetData(d3d.context, (ID3D11Asynchronous *)queries->ts[ts], &timestamp, sizeof(timestamp), 0) == S_FALSE)
        {
            Sleep(1);
        }

        if (ts == 0)
            timestamp_prev = timestamp;

        d3d.timings.dt[ts] = (float)(timestamp - timestamp_prev) / (float)(ts_disjoint.Frequency);
        timestamp_prev = timestamp;
    }

    d3d.query_collect_frame = (d3d.query_collect_frame + 1) % D3D_QUERY_FRAME_COUNT;
}

void d3d11_present()
{
    d3d_queries_t *queries = &d3d.queries[d3d.query_frame];

	AcquireSRWLockExclusive(&d3d.context_lock);

    HRESULT hr = IDXGISwapChain_Present(d3d.swap_chain, 1, 0);

    d3d_timestamp(RENDER_TS_END_FRAME);
    ID3D11DeviceContext_End(d3d.context, (ID3D11Asynchronous *)queries->disjoint);

    d3d_collect_timestamp_data();

	ReleaseSRWLockExclusive(&d3d.context_lock);

    d3d.query_frame = (d3d.query_frame + 1) % D3D_QUERY_FRAME_COUNT;

    if (hr == DXGI_STATUS_OCCLUDED)
        Sleep(16);
}

void get_resolution(int *w, int *h)
{
    *w = d3d.current_width;
    *h = d3d.current_height;
}

resource_handle_t reserve_texture(void)
{
    d3d_texture_t *resource = bd_add(&d3d_textures);
	resource->state = D3D_TEXTURE_STATE_RESERVED;

	resource_handle_t result = bd_get_handle(&d3d_textures, resource);

	return result;
}

void populate_texture(resource_handle_t handle, const upload_texture_t *params)
{
    if (!params->data.pixels)
        return;

    d3d_texture_t *resource = bd_get(&d3d_textures, handle);

	uint32_t state = resource->state;

	if (state > D3D_TEXTURE_STATE_RESERVED)
		return;

    if (resource &&
		atomic_cas_u32(&resource->state, D3D_TEXTURE_STATE_LOADING, state) == state)
    {
        resource->desc = params->desc;

        uint32_t pixel_size = 1;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        switch (params->desc.format)
        {
            case PIXEL_FORMAT_R8:            pixel_size = 1;            format = DXGI_FORMAT_R8_UNORM;            break;
            case PIXEL_FORMAT_RG8:           pixel_size = 2;            format = DXGI_FORMAT_R8G8_UNORM;          break;
            case PIXEL_FORMAT_RGBA8:         pixel_size = 4;            format = DXGI_FORMAT_R8G8B8A8_UNORM;      break;
            case PIXEL_FORMAT_SRGB8_A8:      pixel_size = 4;            format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
            case PIXEL_FORMAT_R11G11B10F:    pixel_size = 4;            format = DXGI_FORMAT_R11G11B10_FLOAT;     break;
            case PIXEL_FORMAT_R32G32B32F:    pixel_size = sizeof(v3_t); format = DXGI_FORMAT_R32G32B32_FLOAT;     break;
            case PIXEL_FORMAT_R32G32B32A32F: pixel_size = sizeof(v4_t); format = DXGI_FORMAT_R32G32B32A32_FLOAT;  break;
            INVALID_DEFAULT_CASE;
        }

        uint32_t pitch = params->data.pitch;

        if (!pitch)  
            pitch = params->desc.w*pixel_size;

        uint32_t slice_pitch = params->data.slice_pitch;

        if (!slice_pitch)  
            slice_pitch = params->desc.w*params->desc.h*pixel_size;

        if (params->desc.type == TEXTURE_TYPE_2D)
        {
            D3D11_TEXTURE2D_DESC desc = {
                .Width      = params->desc.w,
                .Height     = params->desc.h,
                .MipLevels  = 1,
                .ArraySize  = 1,
                .Format     = format,
                .SampleDesc = { 1, 0 },
                .Usage      = D3D11_USAGE_IMMUTABLE,
                .BindFlags  = D3D11_BIND_SHADER_RESOURCE,
            };

            if (params->desc.flags & TEXTURE_FLAG_CUBEMAP)
            {
                desc.ArraySize = 6;
                desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

                D3D11_SUBRESOURCE_DATA data[6];
                for (size_t i = 0; i < 6; i++)
                {
                    data[i] = (D3D11_SUBRESOURCE_DATA){
                        .pSysMem     = params->data.faces[i],
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
				if (params->upload_flags & UPLOAD_TEXTURE_GEN_MIPMAPS)
				{
					desc.Usage      = D3D11_USAGE_DEFAULT;
					desc.MipLevels  = 0;
					desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
					desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

					ID3D11Device_CreateTexture2D(d3d.device, &desc, NULL, &resource->tex[0]);

					D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
						.Format              = desc.Format,
						.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D,
						.Texture2D.MipLevels = UINT_MAX,
					};

					ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)resource->tex[0], &srv_desc, &resource->srv);

					AcquireSRWLockExclusive(&d3d.context_lock);

					ID3D11DeviceContext_UpdateSubresource(d3d.context, (ID3D11Resource *)resource->tex[0], 0, NULL, params->data.pixels, pitch, params->desc.h*pitch);
					ID3D11DeviceContext_GenerateMips(d3d.context, resource->srv);

					ReleaseSRWLockExclusive(&d3d.context_lock);
				}
				else
				{
					D3D11_SUBRESOURCE_DATA data = {
						.pSysMem     = params->data.pixels,
						.SysMemPitch = pitch,
					};

					ID3D11Device_CreateTexture2D(d3d.device, &desc, &data, &resource->tex[0]);

					D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
						.Format              = desc.Format,
						.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D,
						.Texture2D.MipLevels = 1,
					};

					ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)resource->tex[0], &srv_desc, &resource->srv);
				}
            }
        }
        else
        {
            ASSERT(params->desc.type == TEXTURE_TYPE_3D);

			if (params->upload_flags & UPLOAD_TEXTURE_GEN_MIPMAPS)
			{
				ID3D11Device_CreateTexture3D(
					d3d.device,
					(&(D3D11_TEXTURE3D_DESC) {
						.Width     = params->desc.w,
						.Height    = params->desc.h,
						.Depth     = params->desc.d,
						.MipLevels = 1,
						.Format    = format,
						.Usage     = D3D11_USAGE_DEFAULT,
						.Usage     = D3D11_USAGE_IMMUTABLE,
						.BindFlags = D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,
						.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
					}),
					NULL,
					&resource->tex_3d
				);

				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
					.Format              = format,
					.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE3D,
					.Texture3D.MipLevels = UINT_MAX,
				};

				ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)resource->tex_3d, &srv_desc, &resource->srv);

				AcquireSRWLockExclusive(&d3d.context_lock);

				ID3D11DeviceContext_GenerateMips(d3d.context, resource->srv);
				ID3D11DeviceContext_UpdateSubresource(d3d.context, (ID3D11Resource *)resource->tex_3d, 0, NULL, params->data.pixels, pitch, slice_pitch);

				ReleaseSRWLockExclusive(&d3d.context_lock);
			}
			else
			{
				D3D11_SUBRESOURCE_DATA data = {
					.pSysMem          = params->data.pixels,
					.SysMemPitch      = pitch,
					.SysMemSlicePitch = slice_pitch,
				};

				ID3D11Device_CreateTexture3D(
					d3d.device,
					(&(D3D11_TEXTURE3D_DESC) {
						.Width     = params->desc.w,
						.Height    = params->desc.h,
						.Depth     = params->desc.d,
						.MipLevels = 1,
						.Format    = format,
						.Usage     = D3D11_USAGE_IMMUTABLE,
						.BindFlags = D3D11_BIND_SHADER_RESOURCE,
					}),
					&data,
					&resource->tex_3d
				);

				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
					.Format              = format,
					.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE3D,
					.Texture3D.MipLevels = 1,
				};

				ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)resource->tex_3d, &srv_desc, &resource->srv);
			}
        }

		COMPILER_BARRIER;

		resource->state = D3D_TEXTURE_STATE_LOADED;
		WakeByAddressAll(&resource->state);
    }
}

resource_handle_t upload_texture(const upload_texture_t *params)
{
	resource_handle_t result = reserve_texture();
	populate_texture(result, params);

	return result;
}

void destroy_texture(resource_handle_t handle)
{
    d3d_texture_t *resource = bd_get(&d3d_textures, handle);

	if (!resource)
		return;

	while (resource->state == D3D_TEXTURE_STATE_LOADING)
	{
		uint32_t unwanted_state = D3D_TEXTURE_STATE_LOADING;
		WaitOnAddress(&resource->state, &unwanted_state, sizeof(resource->state), INFINITE);
	}

	if (atomic_cas_u32(&resource->state, D3D_TEXTURE_STATE_NONE, D3D_TEXTURE_STATE_LOADED) == D3D_TEXTURE_STATE_LOADED)
	{
		switch (resource->desc.type)
		{
			case TEXTURE_TYPE_2D:
			{
				D3D_SAFE_RELEASE(resource->tex[0]);
				D3D_SAFE_RELEASE(resource->tex[1]);
				D3D_SAFE_RELEASE(resource->tex[2]);
				D3D_SAFE_RELEASE(resource->tex[3]);
				D3D_SAFE_RELEASE(resource->tex[4]);
				D3D_SAFE_RELEASE(resource->tex[5]);
			} break;

			case TEXTURE_TYPE_3D:
			{
				D3D_SAFE_RELEASE(resource->tex_3d);
			} break;
		}

		D3D_SAFE_RELEASE(resource->srv);

		bd_rem(&d3d_textures, handle);
	}
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

            resource->vertex_format = params->vertex_format;
            resource->vcount        = params->vertex_count;

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

void set_model_buffers(d3d_model_t *model, DXGI_FORMAT index_format)
{
    ID3D11DeviceContext_IASetInputLayout(d3d.context, d3d.layouts[model->vertex_format]);
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d.context, model->primitive_topology);
    UINT stride = vertex_format_size[model->vertex_format];
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(d3d.context, 0, 1, &model->vbuffer, &stride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d.context, model->ibuffer, index_format, 0);
}

bool describe_texture(resource_handle_t handle, texture_desc_t *result)
{
    d3d_texture_t *texture = d3d_get_texture_or(handle, d3d.missing_texture);
	copy_struct(result, &texture->desc);

	return texture != d3d.missing_texture;
}

static void get_timings(render_timings_t *timings)
{
    copy_struct(timings, &d3d.timings);
}

render_api_i *d3d11_get_api()
{
    static render_api_i api = {
        .get_resolution   = get_resolution,

        .describe_texture = describe_texture,

		.reserve_texture  = reserve_texture,
		.populate_texture = populate_texture,
        .upload_texture   = upload_texture,
        .destroy_texture  = destroy_texture,

        .upload_model     = upload_model,
        .destroy_model    = destroy_model,

        .get_timings      = get_timings,
    };
    return &api;
}
