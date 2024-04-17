// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

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
// TODO: Add AMD version of this ---^ thing or just enumerate devices by performance

pool_t d3d_models   = INIT_POOL_EX(d3d_model_t,   POOL_FLAGS_CONCURRENT);
pool_t d3d_textures = INIT_POOL_EX(d3d_texture_t, POOL_FLAGS_CONCURRENT);

d3d_state_t d3d;

fn_local ID3D11Buffer *d3d_make_cbuf(size_t size)
{
    ID3D11Buffer *result = NULL;

    D3D11_BUFFER_DESC desc = {
        .ByteWidth      = (UINT)align_forward(size, 16),
        .Usage          = D3D11_USAGE_DYNAMIC,
        .BindFlags      = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };

    ID3D11Device_CreateBuffer(d3d.device, &desc, NULL, &result);
    return result;
}

int init_d3d11(void *hwnd_)
{
    HWND hwnd = hwnd_;

    HRESULT hr = S_OK;

	ID3D11Device *device;
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

		// TODO: Enumerate devices by high performance

        for (size_t i = 0; i < ARRAY_COUNT(driver_types); i++)
        {
            D3D_DRIVER_TYPE driver = driver_types[i];
            hr = D3D11CreateDevice(NULL, driver, NULL, flags, levels, ARRAY_COUNT(levels), D3D11_SDK_VERSION, &device, &picked_level, &d3d.context);

            if (hr == E_INVALIDARG)
            {
                // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it.
                hr = D3D11CreateDevice(NULL, driver, NULL, flags, levels + 1, ARRAY_COUNT(levels) - 1, D3D11_SDK_VERSION, &device, &picked_level, &d3d.context);
            }

            if (SUCCEEDED(hr))
            {
                break;
            }
        }
    }

    if (!device)       return -1;
    if (!d3d.context)  return -1;

	ID3D11Device1 *device1;
	if (SUCCEEDED(ID3D11Device_QueryInterface(device, &IID_ID3D11Device1, &device1)))
	{
		D3D_SAFE_RELEASE(device);
		d3d.device = device1;
	}
	else
	{
		FATAL_ERROR("Failed to query D3D11Device1 interface");
	}

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

#if 0
    // disable alt+enter keybind
    {
        IDXGIFactory *factory;
        IDXGISwapChain1_GetParent(d3d.swap_chain, &IID_IDXGIFactory, &factory);
        IDXGIFactory_MakeWindowAssociation(factory, hwnd, DXGI_MWA_NO_ALT_ENTER);
        IDXGIFactory_Release(factory);
    }
#endif

    // create input layouts

    m_scoped_temp
    {
		// TODO: use tighter packing for all this (normals especially)

        D3D11_INPUT_ELEMENT_DESC layout_pos[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(r_vertex_pos_t, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D11_INPUT_ELEMENT_DESC layout_immediate[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(r_vertex_immediate_t, pos),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, offsetof(r_vertex_immediate_t, tex),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32_UINT,            0, offsetof(r_vertex_immediate_t, col),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(r_vertex_immediate_t, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D11_INPUT_ELEMENT_DESC layout_brush[] = {
            { "POSITION",          0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(r_vertex_brush_t,     pos),          D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",          0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(r_vertex_brush_t,     tex),          D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD_LIGHTMAP", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(r_vertex_brush_t,     tex_lightmap), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",            0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(r_vertex_brush_t,     normal),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        string_t hlsl_file = S("gamedata/shaders/input_layout_nonsense.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        ID3DBlob *vs_pos       = d3d_compile_shader(hlsl_file, hlsl, "pos", "vs_5_0");
        ID3DBlob *vs_immediate = d3d_compile_shader(hlsl_file, hlsl, "immediate", "vs_5_0");
        ID3DBlob *vs_brush     = d3d_compile_shader(hlsl_file, hlsl, "brush", "vs_5_0");

        ID3D11Device_CreateInputLayout(d3d.device, layout_pos, ARRAY_COUNT(layout_pos), 
                                       ID3D10Blob_GetBufferPointer(vs_pos), ID3D10Blob_GetBufferSize(vs_pos), 
                                       &d3d.layouts[R_VERTEX_FORMAT_POS]);

        ID3D11Device_CreateInputLayout(d3d.device, layout_immediate, ARRAY_COUNT(layout_immediate), 
                                       ID3D10Blob_GetBufferPointer(vs_immediate), ID3D10Blob_GetBufferSize(vs_immediate), 
                                       &d3d.layouts[R_VERTEX_FORMAT_IMMEDIATE]);

        ID3D11Device_CreateInputLayout(d3d.device, layout_brush, ARRAY_COUNT(layout_brush), 
                                       ID3D10Blob_GetBufferPointer(vs_brush), ID3D10Blob_GetBufferSize(vs_brush),
                                       &d3d.layouts[R_VERTEX_FORMAT_BRUSH]);

        ID3D10Blob_Release(vs_pos);
        ID3D10Blob_Release(vs_immediate);
        ID3D10Blob_Release(vs_brush);
    }

    // compile shaders

    m_scoped_temp
    {
        string_t hlsl_file = S("gamedata/shaders/brushmodel.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.world_vs = d3d_compile_vs(hlsl_file, hlsl, "vs");
        d3d.world_ps = d3d_compile_ps(hlsl_file, hlsl, "ps");
    }

	static struct { r_immediate_shader_t shader; string_t path; } immediate_shaders[] = {
		{ .shader = R_SHADER_FLAT,           .path = Sc("gamedata/shaders/immediate.hlsl")                },
		{ .shader = R_SHADER_DEBUG_LIGHTING, .path = Sc("gamedata/shaders/immediate_debug_lighting.hlsl") },
		{ .shader = R_SHADER_TEXT,           .path = Sc("gamedata/shaders/immediate_text.hlsl")           },
	};

	STATIC_ASSERT(ARRAY_COUNT(immediate_shaders) == R_SHADER_COUNT, "There's a new immediate shader kind that is not being loaded!");

	for_array(i, immediate_shaders) m_scoped_temp
	{
		string_t path = immediate_shaders[i].path;
		string_t file = fs_read_entire_file(temp, path);
		d3d.immediate_shaders[immediate_shaders[i].shader].vs = d3d_compile_vs(path, file, "vs");
		d3d.immediate_shaders[immediate_shaders[i].shader].ps = d3d_compile_ps(path, file, "ps");
	}

	m_scoped_temp
	{
		string_t path = S("gamedata/shaders/ui_rect.hlsl");
		string_t file = fs_read_entire_file(temp, path);
		d3d.ui_rect_vs = d3d_compile_vs(path, file, "vs");
		d3d.ui_rect_ps = d3d_compile_ps(path, file, "ps");
	}

    m_scoped_temp
    {
        string_t hlsl_file = S("gamedata/shaders/skybox.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.skybox_vs = d3d_compile_vs(hlsl_file, hlsl, "vs");
        d3d.skybox_ps = d3d_compile_ps(hlsl_file, hlsl, "ps");
    }

    m_scoped_temp
    {
        string_t hlsl_file = S("gamedata/shaders/postprocess.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.postprocess_vs  = d3d_compile_vs(hlsl_file, hlsl, "postprocess_vs");
        d3d.msaa_resolve_ps = d3d_compile_ps(hlsl_file, hlsl, "msaa_resolve_ps");
        d3d.hdr_resolve_ps  = d3d_compile_ps(hlsl_file, hlsl, "hdr_resolve_ps");
    }

    m_scoped_temp
    {
        string_t hlsl_file = S("gamedata/shaders/shadowmap.hlsl");
        string_t hlsl = fs_read_entire_file(temp, hlsl_file);

        d3d.shadowmap_vs = d3d_compile_vs(hlsl_file, hlsl, "shadowmap_vs");
    }

    // create constant buffers

    d3d.cbuf_view  = d3d_make_cbuf(sizeof(d3d_cbuf_view_t));
    d3d.cbuf_model = d3d_make_cbuf(sizeof(d3d_cbuf_model_t));
    d3d.cbuf_imm   = d3d_make_cbuf(sizeof(d3d_cbuf_imm_t));

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
        D3D11_RASTERIZER_DESC1 desc = {
			.FrontCounterClockwise = true,
            .FillMode              = D3D11_FILL_SOLID,
            .CullMode              = D3D11_CULL_BACK,
            .ScissorEnable         = TRUE,
            .AntialiasedLineEnable = TRUE,
        };
        ID3D11Device1_CreateRasterizerState1(d3d.device, &desc, &d3d.rs);
    }

    // cull front rasterizer

    {
        D3D11_RASTERIZER_DESC1 desc = {
			.FrontCounterClockwise = true,
            .FillMode              = D3D11_FILL_SOLID,
            .CullMode              = D3D11_CULL_FRONT,
            .ScissorEnable         = TRUE,
            .AntialiasedLineEnable = TRUE,
        };
        ID3D11Device1_CreateRasterizerState1(d3d.device, &desc, &d3d.rs_cull_front);
    }

    // create no-cull rasterizer state

    {
        D3D11_RASTERIZER_DESC1 desc = {
			.FrontCounterClockwise = true,
            .FillMode              = D3D11_FILL_SOLID,
            .CullMode              = D3D11_CULL_NONE,
            .ScissorEnable         = TRUE,
            .AntialiasedLineEnable = TRUE,
        };
        ID3D11Device1_CreateRasterizerState1(d3d.device, &desc, &d3d.rs_no_cull);
    }

    // create depth & stencil

    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable      = TRUE,
            .DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc        = D3D11_COMPARISON_GREATER,
        };
        ID3D11Device1_CreateDepthStencilState(d3d.device, &desc, &d3d.dss);
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

        texture_handle_t handle = upload_texture(&(r_upload_texture_t){
            .desc = {
                .format = R_PIXEL_FORMAT_RGBA8,
                .w      = 1,
                .h      = 1,
            },
            .data = {
                .pixels = &pixel,
            },
        });
        d3d.white_texture = pool_get(&d3d_textures, handle);
    }

    // default "missing texture" texture

    {
        enum { w = R_MISSING_TEXTURE_SIZE, h = R_MISSING_TEXTURE_SIZE };

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

        texture_handle_t handle = upload_texture(&(r_upload_texture_t){
            .desc = {
                .format = R_PIXEL_FORMAT_RGBA8,
                .w      = w,
                .h      = h,
            },
            .data = {
                .pixels = pixels,
            },
        });
        d3d.missing_texture = pool_get(&d3d_textures, handle);

        texture_handle_t cubemap_handle = upload_texture(&(r_upload_texture_t){
            .desc = {
                .format = R_PIXEL_FORMAT_RGBA8,
                .w      = w,
                .h      = h,
                .flags  = R_TEXTURE_FLAG_CUBEMAP,
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
        d3d.missing_texture_cubemap = pool_get(&d3d_textures, cubemap_handle);
    }

    // blue noise texture
    m_scoped_temp
    {
        image_t t0 = load_image_from_disk(temp, S("gamedata/textures/noise/LDR_LLL1_0.png"), 1);
        image_t t1 = load_image_from_disk(temp, S("gamedata/textures/noise/LDR_LLL1_7.png"), 1);
        image_t t2 = load_image_from_disk(temp, S("gamedata/textures/noise/LDR_LLL1_15.png"), 1);
        image_t t3 = load_image_from_disk(temp, S("gamedata/textures/noise/LDR_LLL1_23.png"), 1);

        uint32_t *pixels = m_alloc_array(temp, t0.info.w*t0.info.h, uint32_t);

        unsigned char *t0p = t0.pixels;
        unsigned char *t1p = t1.pixels;
        unsigned char *t2p = t2.pixels;
        unsigned char *t3p = t3.pixels;

        for (size_t i = 0; i < t0.info.w*t0.info.h; i++)
        {
            pixels[i] = ((t0p[i] <<  0) | 
                         (t1p[i] <<  8) |
                         (t2p[i] << 16) |
                         (t3p[i] << 24));
        }

        texture_handle_t handle = render->upload_texture(&(r_upload_texture_t){
            .desc = {
                .format = R_PIXEL_FORMAT_RGBA8,
                .w      = t0.info.w,
                .h      = t0.info.h,
            },
            .data = {
                .pixels = pixels,
            },
        });
        d3d.blue_noise = pool_get(&d3d_textures, handle);
    }

    // create immediate rendering buffers

    {
        size_t stride = sizeof(uint32_t);
        size_t icount = R_IMMEDIATE_INDICES_CAPACITY;

        D3D11_BUFFER_DESC desc = {
            .ByteWidth      = (DWORD)(stride*icount),
            .Usage          = D3D11_USAGE_DYNAMIC,
            .BindFlags      = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(d3d.device, &desc, NULL, &d3d.immediate_ibuffer);
    }

    {
        size_t stride = r_vertex_format_size[R_VERTEX_FORMAT_IMMEDIATE];
        size_t vcount = R_IMMEDIATE_VERTICES_CAPACITY;

        D3D11_BUFFER_DESC desc = {
            .ByteWidth      = (DWORD)(stride*vcount),
            .Usage          = D3D11_USAGE_DYNAMIC,
            .BindFlags      = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(d3d.device, &desc, NULL, &d3d.immediate_vbuffer);
    }

	// create ui rect buffer

    {
        uint32_t stride = sizeof(r_ui_rect_t);
        uint32_t count  = R_UI_RECTS_CAPACITY;

        D3D11_BUFFER_DESC desc = {
            .ByteWidth           = (DWORD)(stride*count),
            .Usage               = D3D11_USAGE_DYNAMIC,
            .BindFlags           = D3D11_BIND_SHADER_RESOURCE,
            .CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE,
			.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
			.StructureByteStride = stride,
        };

        ID3D11Device_CreateBuffer(d3d.device, &desc, NULL, &d3d.ui_rect_buffer);
		ID3D11Device_CreateShaderResourceView(d3d.device, (ID3D11Resource *)d3d.ui_rect_buffer, (&(D3D11_SHADER_RESOURCE_VIEW_DESC){
			.Format        = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
			.Buffer = {
				.FirstElement = 0,
				.NumElements  = R_UI_RECTS_CAPACITY,
			},
		}), &d3d.ui_rect_srv);
    }

    // create skybox model

    {
        mesh_handle_t handle = upload_mesh(&(r_upload_mesh_t){
            .vertex_format = R_VERTEX_FORMAT_POS,
            .vertex_count  = ARRAY_COUNT(g_skybox_vertices) / 3,
            .vertices      = g_skybox_vertices,
        });
        d3d.skybox_model = pool_get(&d3d_models, handle);
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

    ID3D11RasterizerState1 *rs = d3d.rs_no_cull;

    switch (pass->cull)
    {
        case R_CULL_BACK:  rs = d3d.rs; break;
        case R_CULL_FRONT: rs = d3d.rs_cull_front; break;
    }

    ID3D11DeviceContext_RSSetState(d3d.context, (ID3D11RasterizerState *)rs);

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

static d3d_texture_t *d3d_get_texture_or(texture_handle_t handle, d3d_texture_t *fallback)
{
	d3d_texture_t *texture = pool_get(&d3d_textures, handle);

	// TODO: Review sanity of the thread safety of D3D_TEXTURE_STATE_DESTROY_PENDING
	if (!texture || (texture->state != D3D_TEXTURE_STATE_LOADED && texture->state != D3D_TEXTURE_STATE_DESTROY_PENDING))
		texture = fallback;

	return texture;
}

void d3d_do_post_pass(const d3d_post_pass_t *pass)
{
    // set output merger state
    ID3D11DeviceContext_OMSetBlendState(d3d.context, NULL, NULL, ~0U);
    ID3D11DeviceContext_OMSetDepthStencilState(d3d.context, d3d.dss_no_depth, 0);
    ID3D11DeviceContext_OMSetRenderTargets(d3d.context, 1, &pass->render_target, NULL);

    ID3D11DeviceContext_IASetInputLayout(d3d.context, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d.context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // set vertex shader
    ID3D11DeviceContext_VSSetConstantBuffers(d3d.context, 0, 1, &d3d.cbuf_view);
    ID3D11DeviceContext_VSSetShader(d3d.context, d3d.postprocess_vs, NULL, 0);

    // set rasterizer state
    ID3D11DeviceContext_RSSetViewports(d3d.context, 1, &pass->viewport);
    ID3D11DeviceContext_RSSetState(d3d.context, (ID3D11RasterizerState *)d3d.rs_no_cull);
    ID3D11DeviceContext_RSSetScissorRects(d3d.context, 1, (&(D3D11_RECT){ 0, 0, d3d.current_width, d3d.current_height }));

    // set pixel shader
    ID3D11DeviceContext_PSSetConstantBuffers(d3d.context, 0, 1, &d3d.cbuf_view);
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

fn_local void radix_sort_commands(r_command_t *array, size_t count)
{
    m_scoped_temp
    {
        r_command_t *src = array;
        r_command_t *dst = m_alloc_array(temp, count, r_command_t);

        for (size_t byte_index = 0; byte_index < sizeof(*array); byte_index++)
        {
            size_t offsets[256] = { 0 };

            for (size_t i = 0; i < count; i++)
            {
                unsigned char radix_piece = (unsigned char)(src[i].key.value >> 8*byte_index);
                offsets[radix_piece]++;
            }

            uint64_t total = 0;
            for (size_t i = 0; i < ARRAY_COUNT(offsets); i++)
            {
                uint64_t piece_count = offsets[i];
                offsets[i] = total;
                total += piece_count;
            }

            for (size_t i = 0; i < count; i++)
            {
                unsigned char radix_piece = (unsigned char)(src[i].key.value >> 8*byte_index);
                dst[offsets[radix_piece]++] = src[i];
            }

            SWAP(r_command_t *, dst, src);
        }
    }
}

fn_local void d3d_update_view_constants(const r_view_t *view)
{
    v3_t sun_direction = view->scene.sun_direction;

    m4x4_t sun_view   = make_view_matrix(add(view->camera_p, mul(-256.0f, sun_direction)), 
                                         negate(sun_direction), 
                                         make_v3(0, 0, 1));

    m4x4_t sun_ortho  = make_orthographic_matrix(2048, 2048, 512);
    m4x4_t sun_matrix = mul(sun_ortho, sun_view);

    m4x4_t skybox_view = view->view_matrix;

    skybox_view.e[3][0] = 0.0f;
    skybox_view.e[3][1] = 0.0f;
    skybox_view.e[3][2] = 0.0f;

    m4x4_t swizzle = {
        1, 0, 0, 0,
        0, 0, 1, 0,
        0, 1, 0, 0,
        0, 0, 0, 1,
    };

    m4x4_t skybox_matrix = M4X4_IDENTITY;
    skybox_matrix = mul(skybox_matrix, view->proj_matrix);
    skybox_matrix = mul(skybox_matrix, skybox_view);
    skybox_matrix = mul(skybox_matrix, swizzle);

    const r_scene_parameters_t *scene = &view->scene;

    const d3d_cbuf_view_t cbuf_view = {
        .view_matrix     = view->view_matrix,
        .proj_matrix     = view->proj_matrix,
        .sun_matrix      = sun_matrix,
        .skybox_matrix   = skybox_matrix,
        .sun_direction   = scene->sun_direction,
        .sun_color       = scene->sun_color,
        .light_direction = sun_direction,
        .fog_offset      = scene->fog_offset,
        .fog_dim         = scene->fog_dim,
        .screen_dim      = { (float)d3d.current_width, (float)d3d.current_height },
        .fog_density     = scene->fog_density,
        .fog_absorption  = scene->fog_absorption,
        .fog_scattering  = scene->fog_scattering,
        .fog_phase_k     = scene->fog_phase_k,
        .frame_index     = d3d.frame_index,
    };

    update_buffer(d3d.cbuf_view, &cbuf_view, sizeof(cbuf_view));
}

fn_local void d3d_render_skybox(const r_view_t *view, D3D11_VIEWPORT viewport)
{
    const r_scene_parameters_t *scene = &view->scene;

    if (!RESOURCE_HANDLE_VALID(scene->skybox))
        return;

    d3d_model_t   *model   = d3d.skybox_model;
    d3d_texture_t *texture = d3d_get_texture_or(scene->skybox, d3d.missing_texture_cubemap);

    render_model(&(render_pass_t) {
        .render_target = d3d.scene_target.color_rtv,
        .depth_stencil = d3d.scene_target.depth_dsv,

        .model = model,

        .vs = d3d.skybox_vs,
        .ps = d3d.skybox_ps,

        .index_format = DXGI_FORMAT_R16_UINT,

        .cbuffer_count = 1,
        .cbuffers      = (ID3D11Buffer *[]) { d3d.cbuf_view },
        .srv_count     = 1,
        .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv, },

        .depth         = D3D_DEPTH_TEST_NONE,
        .cull          = R_CULL_NONE,
        .sample_linear = true,
        .viewport      = viewport,
    });
}

fn_local size_t d3d_render_view(const d3d_frame_context_t *frame, D3D11_VIEWPORT viewport, const r_command_buffer_t *commands, size_t first_command_index)
{
    const r_command_t          *first_command = &commands->commands[first_command_index];
    const r_command_key_t      *first_key     = &first_command->key;
    const r_view_index_t        view_index    = (r_view_index_t)first_key->view;
    const r_view_t             *view          = &commands->views[view_index];
    const d3d_view_data_t      *view_data     = &frame->view_datas[view_index];
    const r_scene_parameters_t *scene         = &view->scene;

    d3d_update_view_constants(view);

    // TODO: Should render skybox last for efficiency (can reject anything not at infinite Z)
    d3d_render_skybox(view, viewport);

    size_t command_index = first_command_index;
    for (; command_index < commands->commands_count; command_index++)
    {
        r_command_t *command = &commands->commands[command_index];
        r_command_key_t *key = &command->key;

        bool should_resolve = (command_index == view_data->last_scene_command);
        if (should_resolve)
        {
            // Done rendering scene view, apply post processing / MSAA resolve

            d3d_timestamp(RENDER_TS_SCENE);

            d3d_texture_t *fogmap = d3d_get_texture_or(scene->fogmap, NULL);

            ID3D11ShaderResourceView *fogmap_srv = fogmap ? fogmap->srv : NULL;

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
        }

        if (key->view != view_index)
        {
            break;
        }

        const r_view_layer_t view_layer = key->view_layer;
        const bool is_scene = (view_layer == R_VIEW_LAYER_SCENE);

        const d3d_rendertarget_t *rt = is_scene ? &d3d.scene_target : &d3d.backbuffer;

        const r_command_kind_t kind = key->kind;

        switch (kind)
        {
            case R_COMMAND_MODEL:
            {
                r_command_model_t *data = command->data;

                d3d_model_t *model = pool_get(&d3d_models, data->mesh);
                if (model)
                {
                    ASSERT(data->material->kind == R_MATERIAL_BRUSH);
                    const r_material_brush_t *material = (void *)data->material;
                    d3d_texture_t *texture  = d3d_get_texture_or(material->texture, d3d.missing_texture);
                    d3d_texture_t *lightmap = d3d_get_texture_or(material->lightmap, d3d.white_texture);

                    const d3d_cbuf_model_t cbuf_model = {
                        .model_matrix = data->transform,
                    };

                    update_buffer(d3d.cbuf_model, &cbuf_model, sizeof(cbuf_model));

                    render_model(&(render_pass_t) {
                        .render_target = rt->color_rtv,
                        .depth_stencil = rt->depth_dsv,

                        .model = model,

                        .index_format = DXGI_FORMAT_R16_UINT,

                        .vs = d3d.world_vs,
                        .ps = d3d.world_ps,

                        .cbuffer_count = 2,
                        .cbuffers      = (ID3D11Buffer *[]) { d3d.cbuf_view, d3d.cbuf_model },
                        .srv_count     = 3,
                        .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv, lightmap->srv, d3d.sun_shadowmap.depth_srv, },

                        .depth         = D3D_DEPTH_TEST_GREATER,
                        .cull          = R_CULL_BACK,
                        .sample_linear = false,
                        .viewport      = viewport,
                    });
                }
            } break;

            case R_COMMAND_IMMEDIATE:
            {
                r_immediate_t *data = command->data;

                const d3d_cbuf_model_t cbuf_model = {
                    .model_matrix    = data->transform,
                    .depth_bias      = data->depth_bias,
                };

                update_buffer(d3d.cbuf_model, &cbuf_model, sizeof(cbuf_model));

                D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                switch (data->topology)
                {
                    case R_TOPOLOGY_TRIANGLELIST:  topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
                    case R_TOPOLOGY_TRIANGLESTRIP: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
                    case R_TOPOLOGY_LINELIST:      topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
                    case R_TOPOLOGY_LINESTRIP:     topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
                    case R_TOPOLOGY_POINTLIST:     topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
                    INVALID_DEFAULT_CASE;
                }

                // TODO: index/vertex count is specified here, but the offset is in the pass. feels a bit janky.
                d3d_model_t immediate_model = {
                    .vertex_format      = R_VERTEX_FORMAT_IMMEDIATE,
                    .primitive_topology = topology,
                    .icount             = data->icount,
                    .ibuffer            = d3d.immediate_ibuffer,
                    .vcount             = data->vcount,
                    .vbuffer            = d3d.immediate_vbuffer,
                };

                d3d_texture_t *texture = d3d_get_texture_or(data->texture, d3d.white_texture);

                rect2_t clip_rect = rect2_intersect(data->clip_rect, view->clip_rect);

                render_model(&(render_pass_t) {
                    .render_target = rt->color_rtv,
                    .depth_stencil = rt->depth_dsv,

                    .model = &immediate_model,

                    .vs = d3d.immediate_shaders[data->shader].vs,
                    .ps = d3d.immediate_shaders[data->shader].ps,

                    .blend_mode   = data->blend_mode,
                    .index_format = DXGI_FORMAT_R32_UINT,

                    .ioffset = data->ioffset,
                    .voffset = data->voffset,

                    .cbuffer_count = 2,
                    .cbuffers      = (ID3D11Buffer *[]) { d3d.cbuf_view, d3d.cbuf_model },
                    .srv_count     = 1,
                    .srvs          = (ID3D11ShaderResourceView *[]) { texture->srv },

                    .depth    = data->use_depth ? D3D_DEPTH_TEST_GREATER : D3D_DEPTH_TEST_NONE,
                    .cull     = data->cull_mode,
                    .viewport = viewport,
                    .scissor  = true,
                    .scissor_rect = {
                        .left   = (LONG)clip_rect.min.x,
                        .right  = (LONG)clip_rect.max.x,
                        .bottom = (LONG)viewport.Height - (LONG)clip_rect.min.y,
                        .top    = (LONG)viewport.Height - (LONG)clip_rect.max.y,
                    },
                });
            } break;

            case R_COMMAND_UI_RECTS:
            {
                r_command_ui_rects_t *data = command->data;

                const d3d_cbuf_model_t cbuf_model = {
                    .instance_offset = data->first,
                };

                update_buffer(d3d.cbuf_model, &cbuf_model, sizeof(cbuf_model));

                rect2_t clip_rect = view->clip_rect;

                D3D11_RECT scissor_rect = {
                    .left   = (LONG)clip_rect.min.x,
                    .right  = (LONG)clip_rect.max.x,
                    .bottom = (LONG)viewport.Height - (LONG)clip_rect.min.y,
                    .top    = (LONG)viewport.Height - (LONG)clip_rect.max.y,
                };

                d3d_texture_t *texture = d3d_get_texture_or(data->texture, d3d.white_texture);

                // set output merger state
                ID3D11DeviceContext_OMSetBlendState(d3d.context, d3d.bs, NULL, ~0U);
                ID3D11DeviceContext_OMSetDepthStencilState(d3d.context, d3d.dss_no_depth, 0);
                ID3D11DeviceContext_OMSetRenderTargets(d3d.context, 1, &rt->color_rtv, NULL);

                // input assembly
                ID3D11DeviceContext_IASetInputLayout(d3d.context, NULL);
                ID3D11DeviceContext_IASetPrimitiveTopology(d3d.context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

                // set vertex shader
                ID3D11DeviceContext_VSSetConstantBuffers(d3d.context, 0, 1, &d3d.cbuf_view);
                ID3D11DeviceContext_VSSetShaderResources(d3d.context, 0, 2, ((ID3D11ShaderResourceView *[]){ d3d.ui_rect_srv, texture->srv }));
                ID3D11DeviceContext_VSSetShader(d3d.context, d3d.ui_rect_vs, NULL, 0);

                // set rasterizer state
                ID3D11DeviceContext_RSSetViewports(d3d.context, 1, &viewport);
                ID3D11DeviceContext_RSSetState(d3d.context, (ID3D11RasterizerState *)d3d.rs_no_cull);
                ID3D11DeviceContext_RSSetScissorRects(d3d.context, 1, &scissor_rect);

                // set pixel shader
                ID3D11DeviceContext_PSSetConstantBuffers(d3d.context, 0, 2, ((ID3D11Buffer *[]){ d3d.cbuf_view, d3d.cbuf_model }));
                ID3D11DeviceContext_PSSetShaderResources(d3d.context, 0, 2, ((ID3D11ShaderResourceView *[]){ d3d.ui_rect_srv, texture->srv }));
                ID3D11DeviceContext_PSSetSamplers(d3d.context, 0, D3D_SAMPLER_COUNT, d3d.samplers);
                ID3D11DeviceContext_PSSetShader(d3d.context, d3d.ui_rect_ps, NULL, 0);

                ID3D11DeviceContext_DrawInstanced(d3d.context, 4, data->count, 0, 0);
            } break;

            INVALID_DEFAULT_CASE;
        }
    }

    return command_index;
}

void d3d11_execute_command_buffer(r_command_buffer_t *command_buffer, int width, int height)
{
	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

	AcquireSRWLockExclusive(&d3d.context_lock);

    d3d_queries_t *queries = &d3d.queries[d3d.query_frame];
    ID3D11DeviceContext_Begin(d3d.context, (ID3D11Asynchronous *)queries->disjoint);
    d3d_timestamp(RENDER_TS_BEGIN_FRAME);

    d3d_ensure_swap_chain_size(width, height);

    {
        r_command_t *commands       = command_buffer->commands;
        const size_t commands_count = command_buffer->commands_count;
        radix_sort_commands(commands, commands_count);
    }

    const r_command_t *commands       = command_buffer->commands;
    const size_t       commands_count = command_buffer->commands_count;

    // per-frame rendering related context that's useful to pass down to functions
    d3d_frame_context_t frame = {
        .view_datas = m_alloc_array(temp, command_buffer->views_count, d3d_view_data_t),
    };

    for (size_t command_index = 1; command_index < commands_count; command_index++)
    {
        const r_command_t *command = &commands[command_index];
        const r_command_key_t *key = &command->key;

        const r_view_index_t view_index = (r_view_index_t)key->view;

        d3d_view_data_t *view_data = &frame.view_datas[view_index];
        if (key->view_layer == R_VIEW_LAYER_SCENE)
        {
            if (view_data->first_scene_command == 0)
            {
                view_data->first_scene_command = (uint32_t)command_index;
            }

            view_data->last_scene_command = (uint32_t)command_index;
        }
    }

    if (d3d.backbuffer.color_rtv)
    {
        const r_view_t *view = NULL;

        //
        // sun shadows
        //

        ID3D11DeviceContext_ClearDepthStencilView(d3d.context, d3d.sun_shadowmap.depth_dsv, 
                                                  D3D11_CLEAR_DEPTH, 0.0f, 0);
        d3d_timestamp(RENDER_TS_CLEAR_SHADOWMAP);


        D3D11_VIEWPORT sun_viewport =
        {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width    = SUN_SHADOWMAP_RESOLUTION,
            .Height   = SUN_SHADOWMAP_RESOLUTION,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };

        D3D11_RECT sun_scissor_rect = { 0, 0, SUN_SHADOWMAP_RESOLUTION, SUN_SHADOWMAP_RESOLUTION };

        for (size_t command_index = 1; command_index < commands_count; command_index++)
        {
            const r_command_t *command = &commands[command_index];
            const r_command_key_t *key = &command->key;

            const r_view_t *new_view = &command_buffer->views[key->view];
            if (view != new_view)
            {
                view = new_view;
                d3d_update_view_constants(view);
            }
            
            const r_screen_layer_t new_layer = key->screen_layer;
            if (new_layer != R_SCREEN_LAYER_SCENE)
            {
                break;
            }

            const r_view_layer_t view_layer = key->view_layer;
            if (view_layer != R_VIEW_LAYER_SCENE)
            {
                continue;
            }

            const r_command_kind_t kind = key->kind;

            switch (kind)
            {
                case R_COMMAND_MODEL:
                {
                    const r_command_model_t *data = command->data;

                    const d3d_model_t *model = pool_get(&d3d_models, data->mesh);

                    if (model)
                    {
                        d3d_cbuf_model_t cbuf_model = {
                            .model_matrix = data->transform,
                        };

                        update_buffer(d3d.cbuf_model, &cbuf_model, sizeof(cbuf_model));

                        render_model(&(render_pass_t) {
                            .render_target = NULL,
                            .depth_stencil = d3d.sun_shadowmap.depth_dsv,

                            .model = model,

                            .index_format = DXGI_FORMAT_R16_UINT,

                            .vs = d3d.shadowmap_vs,
                            .ps = NULL,

                            .cbuffer_count = 2,
                            .cbuffers      = (ID3D11Buffer *[]) { d3d.cbuf_view, d3d.cbuf_model },

                            .depth         = D3D_DEPTH_TEST_GREATER,
                            .cull          = R_CULL_BACK,
                            .viewport      = sun_viewport,
                            .scissor       = true,
                            .scissor_rect  = sun_scissor_rect,
                        });
                    }
                } break;

                case R_COMMAND_IMMEDIATE:
                {
                    /* skip */
                } break;

                case R_COMMAND_UI_RECTS:
                {
                    /* skip */
                } break;

                INVALID_DEFAULT_CASE;
            }
        }

        d3d_timestamp(RENDER_TS_RENDER_SHADOWMAP);

        //
        // render scene
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

        // update immediate index/vertex buffer
        update_buffer(d3d.immediate_ibuffer, command_buffer->imm_indices,  sizeof(command_buffer->imm_indices[0])*command_buffer->imm_indices_count);
        update_buffer(d3d.immediate_vbuffer, command_buffer->imm_vertices, sizeof(command_buffer->imm_vertices[0])*command_buffer->imm_vertices_count);

        // update ui rect buffer
        update_buffer(d3d.ui_rect_buffer, command_buffer->ui_rects, sizeof(command_buffer->ui_rects[0])*command_buffer->ui_rects_count);

        d3d_timestamp(RENDER_TS_UPLOAD_IMMEDIATE_DATA);

        size_t command_index = 1;
        while (command_index < commands_count)
        {
            command_index = d3d_render_view(&frame, viewport, command_buffer, command_index);
        }

        d3d_timestamp(RENDER_TS_UI);
    }

	ReleaseSRWLockExclusive(&d3d.context_lock);

	// TODO: Review thread-safety of this 
	// and maybe replace it with a queue of destroy commands
	for (pool_iter_t it = pool_iter(&d3d_textures);
		 pool_iter_valid(&it);
		 pool_iter_next(&it))
	{
		d3d_texture_t *resource = it.data;

		d3d_texture_state_t state = resource->state;
		if (state == D3D_TEXTURE_STATE_DESTROY_PENDING &&
			atomic_cas_u32(&resource->state, D3D_TEXTURE_STATE_NONE, D3D_TEXTURE_STATE_DESTROY_PENDING) == D3D_TEXTURE_STATE_NONE)
		{
			switch (resource->desc.type)
			{
				case R_TEXTURE_TYPE_2D:
				{
					D3D_SAFE_RELEASE(resource->tex[0]);
					D3D_SAFE_RELEASE(resource->tex[1]);
					D3D_SAFE_RELEASE(resource->tex[2]);
					D3D_SAFE_RELEASE(resource->tex[3]);
					D3D_SAFE_RELEASE(resource->tex[4]);
					D3D_SAFE_RELEASE(resource->tex[5]);
				} break;

				case R_TEXTURE_TYPE_3D:
				{
					D3D_SAFE_RELEASE(resource->tex_3d);
				} break;
			}

			D3D_SAFE_RELEASE(resource->srv);

			pool_rem_item(&d3d_textures, resource);
		}
	}

    d3d.frame_index++;

    m_scope_end(temp);
}

fn_local void d3d_collect_timestamp_data(void)
{
    if (d3d.query_collect_frame < 0)
    {
        d3d.query_collect_frame += 1;
        return;
    }

    d3d_queries_t *queries = &d3d.queries[d3d.query_collect_frame];

    while (ID3D11DeviceContext_GetData(d3d.context, (ID3D11Asynchronous *)queries->disjoint, NULL, 0, 0) == S_FALSE)
    {
        // debug_print("Snoozin on da query...\n");
        Sleep(1);
    }

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

texture_handle_t reserve_texture(void)
{
    d3d_texture_t *resource = pool_add(&d3d_textures);
	resource->state = D3D_TEXTURE_STATE_RESERVED;

	texture_handle_t result = CAST_HANDLE(texture_handle_t, pool_get_handle(&d3d_textures, resource));

	return result;
}

void populate_texture(texture_handle_t handle, const r_upload_texture_t *params)
{
    if (!params->data.pixels)
        return;

    d3d_texture_t *resource = pool_get(&d3d_textures, handle);

	if (NEVER(!resource))
		return;

	uint32_t state = resource->state;

	if (state != D3D_TEXTURE_STATE_RESERVED)
		return;

    if (resource &&
		atomic_cas_u32(&resource->state, D3D_TEXTURE_STATE_LOADING, state) == state)
    {
        resource->desc = params->desc;

        uint32_t pixel_size = 1;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        switch (params->desc.format)
        {
            case R_PIXEL_FORMAT_R8:            pixel_size = 1;            format = DXGI_FORMAT_R8_UNORM;            break;
            case R_PIXEL_FORMAT_RG8:           pixel_size = 2;            format = DXGI_FORMAT_R8G8_UNORM;          break;
            case R_PIXEL_FORMAT_RGBA8:         pixel_size = 4;            format = DXGI_FORMAT_R8G8B8A8_UNORM;      break;
            case R_PIXEL_FORMAT_SRGB8_A8:      pixel_size = 4;            format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
            case R_PIXEL_FORMAT_R11G11B10F:    pixel_size = 4;            format = DXGI_FORMAT_R11G11B10_FLOAT;     break;
            case R_PIXEL_FORMAT_R32G32B32F:    pixel_size = sizeof(v3_t); format = DXGI_FORMAT_R32G32B32_FLOAT;     break;
            case R_PIXEL_FORMAT_R32G32B32A32F: pixel_size = sizeof(v4_t); format = DXGI_FORMAT_R32G32B32A32_FLOAT;  break;
            INVALID_DEFAULT_CASE;
        }

        uint32_t pitch = params->data.pitch;

        if (!pitch)  
            pitch = params->desc.w*pixel_size;

        uint32_t slice_pitch = params->data.slice_pitch;

        if (!slice_pitch)  
            slice_pitch = params->desc.w*params->desc.h*pixel_size;

        if (params->desc.type == R_TEXTURE_TYPE_2D)
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

            if (params->desc.flags & R_TEXTURE_FLAG_CUBEMAP)
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
				if (params->upload_flags & R_UPLOAD_TEXTURE_GEN_MIPMAPS)
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
            ASSERT(params->desc.type == R_TEXTURE_TYPE_3D);

			if (params->upload_flags & R_UPLOAD_TEXTURE_GEN_MIPMAPS)
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

		if (resource->tex[0] && !string_empty(params->debug_name))
		{
			ID3D11Device_SetPrivateData(d3d.device, &WKPDID_D3DDebugObjectName, (UINT)params->debug_name.count, params->debug_name.data);
		}
    }
}

texture_handle_t upload_texture(const r_upload_texture_t *params)
{
	texture_handle_t result = reserve_texture();
	populate_texture(result, params);

	return result;
}

void destroy_texture(texture_handle_t handle)
{
    d3d_texture_t *resource = pool_get(&d3d_textures, handle);

	if (!resource)
		return;

	while (resource->state == D3D_TEXTURE_STATE_LOADING)
	{
		uint32_t unwanted_state = D3D_TEXTURE_STATE_LOADING;
		WaitOnAddress(&resource->state, &unwanted_state, sizeof(resource->state), INFINITE);
	}

	if (atomic_cas_u32(&resource->state, D3D_TEXTURE_STATE_DESTROY_PENDING, D3D_TEXTURE_STATE_LOADED) == D3D_TEXTURE_STATE_LOADED)
	{
		// yay
	}
}

mesh_handle_t upload_mesh(const r_upload_mesh_t *params)
{
    mesh_handle_t result = { 0 };

    if (NEVER(params->vertex_format >= R_VERTEX_FORMAT_COUNT))
        return result;

    d3d_model_t *resource = pool_add(&d3d_models);

    if (resource)
    {
        ASSERT(resource->vbuffer == NULL);
        ASSERT(resource->ibuffer == NULL);

        // vbuffer
        {
            switch (params->topology)
            {
                case R_TOPOLOGY_TRIANGLELIST:  resource->primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  break;
                case R_TOPOLOGY_TRIANGLESTRIP: resource->primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
                case R_TOPOLOGY_LINELIST:      resource->primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;      break;
                case R_TOPOLOGY_LINESTRIP:     resource->primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;     break;
                case R_TOPOLOGY_POINTLIST:     resource->primitive_topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;     break;
                INVALID_DEFAULT_CASE;
            }

            resource->vertex_format = params->vertex_format;
            resource->vcount        = params->vertex_count;

            UINT stride = r_vertex_format_size[resource->vertex_format];

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

        result = CAST_HANDLE(mesh_handle_t, pool_get_handle(&d3d_models, resource));
    }

    return result;
}

void destroy_mesh(mesh_handle_t mesh)
{
    IGNORED(mesh);
    FATAL_ERROR("TODO: Implement");
}

ID3DBlob *d3d_compile_shader(string_t hlsl_file, string_t hlsl, const char *entry_point, const char *kind)
{
	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

    UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR|D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined(DEBUG_RENDERER)
    flags |= D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* error;

    ID3DBlob* blob = NULL;
    HRESULT hr = D3DCompile(hlsl.data, hlsl.count, string_null_terminate(temp, hlsl_file).data, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry_point, kind, flags, 0, &blob, &error);
    if (FAILED(hr))
    {
        const char* message = ID3D10Blob_GetBufferPointer(error);
        OutputDebugStringA(message);
        FATAL_ERROR("Failed to compile shader %.*s. (%s, entry point '%s'):\n\n%s", Sx(hlsl_file), kind, entry_point, message);
    }

	m_scope_end(temp);

    return blob;
}

ID3D11PixelShader *d3d_compile_ps(string_t hlsl_file, string_t hlsl, const char *entry_point)
{
    ID3DBlob* blob = d3d_compile_shader(hlsl_file, hlsl, entry_point, "ps_5_0");

    if (!blob)
        return NULL;

    ID3D11PixelShader *result;
    ID3D11Device_CreatePixelShader(d3d.device, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), NULL, &result);

    ID3D10Blob_Release(blob);

    return result;
}

ID3D11VertexShader *d3d_compile_vs(string_t hlsl_file, string_t hlsl, const char *entry_point)
{
    ID3DBlob* blob = d3d_compile_shader(hlsl_file, hlsl, entry_point, "vs_5_0");

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

void set_model_buffers(const d3d_model_t *model, DXGI_FORMAT index_format)
{
    ID3D11DeviceContext_IASetInputLayout(d3d.context, d3d.layouts[model->vertex_format]);
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d.context, model->primitive_topology);
    UINT stride = r_vertex_format_size[model->vertex_format];
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(d3d.context, 0, 1, &model->vbuffer, &stride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d.context, model->ibuffer, index_format, 0);
}

bool describe_texture(texture_handle_t handle, r_texture_desc_t *result)
{
    d3d_texture_t *texture = d3d_get_texture_or(handle, d3d.missing_texture);
	copy_struct(result, &texture->desc);

	return texture != d3d.missing_texture;
}

static void get_timings(r_timings_t *timings)
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

        .upload_mesh      = upload_mesh,
        .destroy_mesh     = destroy_mesh,

        .get_timings      = get_timings,
    };
    return &api;
}
