// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

DEFINE_HANDLE_TYPE(texture_handle_t);
DEFINE_HANDLE_TYPE(mesh_handle_t);

#define NULL_TEXTURE_HANDLE ((texture_handle_t){0})
#define NULL_MESH_HANDLE ((mesh_handle_t){0})

typedef unsigned char r_view_index_t;

typedef enum pixel_format_t 
{
	PixelFormat_unknown                    = 0,
	PixelFormat_r32g32b32a32_typeless      = 1,
	PixelFormat_r32g32b32a32_float         = 2,
	PixelFormat_r32g32b32a32_uint          = 3,
	PixelFormat_r32g32b32a32_sint          = 4,
	PixelFormat_r32g32b32_typeless         = 5,
	PixelFormat_r32g32b32_float            = 6,
	PixelFormat_r32g32b32_uint             = 7,
	PixelFormat_r32g32b32_sint             = 8,
	PixelFormat_r16g16b16a16_typeless      = 9,
	PixelFormat_r16g16b16a16_float         = 10,
	PixelFormat_r16g16b16a16_unorm         = 11,
	PixelFormat_r16g16b16a16_uint          = 12,
	PixelFormat_r16g16b16a16_snorm         = 13,
	PixelFormat_r16g16b16a16_sint          = 14,
	PixelFormat_r32g32_typeless            = 15,
	PixelFormat_r32g32_float               = 16,
	PixelFormat_r32g32_uint                = 17,
	PixelFormat_r32g32_sint                = 18,
	PixelFormat_r32g8x24_typeless          = 19,
	PixelFormat_d32_float_s8x24_uint       = 20,
	PixelFormat_r32_float_x8x24_typeless   = 21,
	PixelFormat_x32_typeless_g8x24_uint    = 22,
	PixelFormat_r10g10b10a2_typeless       = 23,
	PixelFormat_r10g10b10a2_unorm          = 24,
	PixelFormat_r10g10b10a2_uint           = 25,
	PixelFormat_r11g11b10_float            = 26,
	PixelFormat_r8g8b8a8_typeless          = 27,
	PixelFormat_r8g8b8a8_unorm             = 28,
	PixelFormat_r8g8b8a8_unorm_srgb        = 29,
	PixelFormat_r8g8b8a8_uint              = 30,
	PixelFormat_r8g8b8a8_snorm             = 31,
	PixelFormat_r8g8b8a8_sint              = 32,
	PixelFormat_r16g16_typeless            = 33,
	PixelFormat_r16g16_float               = 34,
	PixelFormat_r16g16_unorm               = 35,
	PixelFormat_r16g16_uint                = 36,
	PixelFormat_r16g16_snorm               = 37,
	PixelFormat_r16g16_sint                = 38,
	PixelFormat_r32_typeless               = 39,
	PixelFormat_d32_float                  = 40,
	PixelFormat_r32_float                  = 41,
	PixelFormat_r32_uint                   = 42,
	PixelFormat_r32_sint                   = 43,
	PixelFormat_r24g8_typeless             = 44,
	PixelFormat_d24_unorm_s8_uint          = 45,
	PixelFormat_r24_unorm_x8_typeless      = 46,
	PixelFormat_x24_typeless_g8_uint       = 47,
	PixelFormat_r8g8_typeless              = 48,
	PixelFormat_r8g8_unorm                 = 49,
	PixelFormat_r8g8_uint                  = 50,
	PixelFormat_r8g8_snorm                 = 51,
	PixelFormat_r8g8_sint                  = 52,
	PixelFormat_r16_typeless               = 53,
	PixelFormat_r16_float                  = 54,
	PixelFormat_d16_unorm                  = 55,
	PixelFormat_r16_unorm                  = 56,
	PixelFormat_r16_uint                   = 57,
	PixelFormat_r16_snorm                  = 58,
	PixelFormat_r16_sint                   = 59,
	PixelFormat_r8_typeless                = 60,
	PixelFormat_r8_unorm                   = 61,
	PixelFormat_r8_uint                    = 62,
	PixelFormat_r8_snorm                   = 63,
	PixelFormat_r8_sint                    = 64,
	PixelFormat_a8_unorm                   = 65,
	PixelFormat_r1_unorm                   = 66,
	PixelFormat_r9g9b9e5_sharedexp         = 67,
	PixelFormat_r8g8_b8g8_unorm            = 68,
	PixelFormat_g8r8_g8b8_unorm            = 69,
	PixelFormat_bc1_typeless               = 70,
	PixelFormat_bc1_unorm                  = 71,
	PixelFormat_bc1_unorm_srgb             = 72,
	PixelFormat_bc2_typeless               = 73,
	PixelFormat_bc2_unorm                  = 74,
	PixelFormat_bc2_unorm_srgb             = 75,
	PixelFormat_bc3_typeless               = 76,
	PixelFormat_bc3_unorm                  = 77,
	PixelFormat_bc3_unorm_srgb             = 78,
	PixelFormat_bc4_typeless               = 79,
	PixelFormat_bc4_unorm                  = 80,
	PixelFormat_bc4_snorm                  = 81,
	PixelFormat_bc5_typeless               = 82,
	PixelFormat_bc5_unorm                  = 83,
	PixelFormat_bc5_snorm                  = 84,
	PixelFormat_b5g6r5_unorm               = 85,
	PixelFormat_b5g5r5a1_unorm             = 86,
	PixelFormat_b8g8r8a8_unorm             = 87,
	PixelFormat_b8g8r8x8_unorm             = 88,
	PixelFormat_r10g10b10_xr_bias_a2_unorm = 89,
	PixelFormat_b8g8r8a8_typeless          = 90,
	PixelFormat_b8g8r8a8_unorm_srgb        = 91,
	PixelFormat_b8g8r8x8_typeless          = 92,
	PixelFormat_b8g8r8x8_unorm_srgb        = 93,
	PixelFormat_bc6h_typeless              = 94,
	PixelFormat_bc6h_uf16                  = 95,
	PixelFormat_bc6h_sf16                  = 96,
	PixelFormat_bc7_typeless               = 97,
	PixelFormat_bc7_unorm                  = 98,
	PixelFormat_bc7_unorm_srgb             = 99,
	PixelFormat_ayuv                       = 100,
	PixelFormat_y410                       = 101,
	PixelFormat_y416                       = 102,
	PixelFormat_nv12                       = 103,
	PixelFormat_p010                       = 104,
	PixelFormat_p016                       = 105,
	PixelFormat_420_opaque                 = 106,
	PixelFormat_yuy2                       = 107,
	PixelFormat_y210                       = 108,
	PixelFormat_y216                       = 109,
	PixelFormat_nv11                       = 110,
	PixelFormat_ai44                       = 111,
	PixelFormat_ia44                       = 112,
	PixelFormat_p8                         = 113,
	PixelFormat_a8p8                       = 114,
	PixelFormat_b4g4r4a4_unorm             = 115,
	PixelFormat_p208                       = 130,
	PixelFormat_v208                       = 131,
	PixelFormat_v408                       = 132,
	PixelFormat_sampler_feedback_min_mip_opaque,
	PixelFormat_sampler_feedback_mip_region_used_opaque,
	PixelFormat_FORCE_UINT                 = 0xffffffff
} pixel_format_t;

fn_local string_t pixel_format_to_string(pixel_format_t pf)
{
	string_t result = S("<invalid pixel format>");

	switch (pf)
	{
		case PixelFormat_unknown: result = S("unknown"); break;
		case PixelFormat_r32g32b32a32_typeless: result = S("r32g32b32a32_typeless"); break;
		case PixelFormat_r32g32b32a32_float: result = S("r32g32b32a32_float"); break;
		case PixelFormat_r32g32b32a32_uint: result = S("r32g32b32a32_uint"); break;
		case PixelFormat_r32g32b32a32_sint: result = S("r32g32b32a32_sint"); break;
		case PixelFormat_r32g32b32_typeless: result = S("r32g32b32_typeless"); break;
		case PixelFormat_r32g32b32_float: result = S("r32g32b32_float"); break;
		case PixelFormat_r32g32b32_uint: result = S("r32g32b32_uint"); break;
		case PixelFormat_r32g32b32_sint: result = S("r32g32b32_sint"); break;
		case PixelFormat_r16g16b16a16_typeless: result = S("r16g16b16a16_typeless"); break;
		case PixelFormat_r16g16b16a16_float: result = S("r16g16b16a16_float"); break;
		case PixelFormat_r16g16b16a16_unorm: result = S("r16g16b16a16_unorm"); break;
		case PixelFormat_r16g16b16a16_uint: result = S("r16g16b16a16_uint"); break;
		case PixelFormat_r16g16b16a16_snorm: result = S("r16g16b16a16_snorm"); break;
		case PixelFormat_r16g16b16a16_sint: result = S("r16g16b16a16_sint"); break;
		case PixelFormat_r32g32_typeless: result = S("r32g32_typeless"); break;
		case PixelFormat_r32g32_float: result = S("r32g32_float"); break;
		case PixelFormat_r32g32_uint: result = S("r32g32_uint"); break;
		case PixelFormat_r32g32_sint: result = S("r32g32_sint"); break;
		case PixelFormat_r32g8x24_typeless: result = S("r32g8x24_typeless"); break;
		case PixelFormat_d32_float_s8x24_uint: result = S("d32_float_s8x24_uint"); break;
		case PixelFormat_r32_float_x8x24_typeless: result = S("r32_float_x8x24_typeless"); break;
		case PixelFormat_x32_typeless_g8x24_uint: result = S("x32_typeless_g8x24_uint"); break;
		case PixelFormat_r10g10b10a2_typeless: result = S("r10g10b10a2_typeless"); break;
		case PixelFormat_r10g10b10a2_unorm: result = S("r10g10b10a2_unorm"); break;
		case PixelFormat_r10g10b10a2_uint: result = S("r10g10b10a2_uint"); break;
		case PixelFormat_r11g11b10_float: result = S("r11g11b10_float"); break;
		case PixelFormat_r8g8b8a8_typeless: result = S("r8g8b8a8_typeless"); break;
		case PixelFormat_r8g8b8a8_unorm: result = S("r8g8b8a8_unorm"); break;
		case PixelFormat_r8g8b8a8_unorm_srgb: result = S("r8g8b8a8_unorm_srgb"); break;
		case PixelFormat_r8g8b8a8_uint: result = S("r8g8b8a8_uint"); break;
		case PixelFormat_r8g8b8a8_snorm: result = S("r8g8b8a8_snorm"); break;
		case PixelFormat_r8g8b8a8_sint: result = S("r8g8b8a8_sint"); break;
		case PixelFormat_r16g16_typeless: result = S("r16g16_typeless"); break;
		case PixelFormat_r16g16_float: result = S("r16g16_float"); break;
		case PixelFormat_r16g16_unorm: result = S("r16g16_unorm"); break;
		case PixelFormat_r16g16_uint: result = S("r16g16_uint"); break;
		case PixelFormat_r16g16_snorm: result = S("r16g16_snorm"); break;
		case PixelFormat_r16g16_sint: result = S("r16g16_sint"); break;
		case PixelFormat_r32_typeless: result = S("r32_typeless"); break;
		case PixelFormat_d32_float: result = S("d32_float"); break;
		case PixelFormat_r32_float: result = S("r32_float"); break;
		case PixelFormat_r32_uint: result = S("r32_uint"); break;
		case PixelFormat_r32_sint: result = S("r32_sint"); break;
		case PixelFormat_r24g8_typeless: result = S("r24g8_typeless"); break;
		case PixelFormat_d24_unorm_s8_uint: result = S("d24_unorm_s8_uint"); break;
		case PixelFormat_r24_unorm_x8_typeless: result = S("r24_unorm_x8_typeless"); break;
		case PixelFormat_x24_typeless_g8_uint: result = S("x24_typeless_g8_uint"); break;
		case PixelFormat_r8g8_typeless: result = S("r8g8_typeless"); break;
		case PixelFormat_r8g8_unorm: result = S("r8g8_unorm"); break;
		case PixelFormat_r8g8_uint: result = S("r8g8_uint"); break;
		case PixelFormat_r8g8_snorm: result = S("r8g8_snorm"); break;
		case PixelFormat_r8g8_sint: result = S("r8g8_sint"); break;
		case PixelFormat_r16_typeless: result = S("r16_typeless"); break;
		case PixelFormat_r16_float: result = S("r16_float"); break;
		case PixelFormat_d16_unorm: result = S("d16_unorm"); break;
		case PixelFormat_r16_unorm: result = S("r16_unorm"); break;
		case PixelFormat_r16_uint: result = S("r16_uint"); break;
		case PixelFormat_r16_snorm: result = S("r16_snorm"); break;
		case PixelFormat_r16_sint: result = S("r16_sint"); break;
		case PixelFormat_r8_typeless: result = S("r8_typeless"); break;
		case PixelFormat_r8_unorm: result = S("r8_unorm"); break;
		case PixelFormat_r8_uint: result = S("r8_uint"); break;
		case PixelFormat_r8_snorm: result = S("r8_snorm"); break;
		case PixelFormat_r8_sint: result = S("r8_sint"); break;
		case PixelFormat_a8_unorm: result = S("a8_unorm"); break;
		case PixelFormat_r1_unorm: result = S("r1_unorm"); break;
		case PixelFormat_r9g9b9e5_sharedexp: result = S("r9g9b9e5_sharedexp"); break;
		case PixelFormat_r8g8_b8g8_unorm: result = S("r8g8_b8g8_unorm"); break;
		case PixelFormat_g8r8_g8b8_unorm: result = S("g8r8_g8b8_unorm"); break;
		case PixelFormat_bc1_typeless: result = S("bc1_typeless"); break;
		case PixelFormat_bc1_unorm: result = S("bc1_unorm"); break;
		case PixelFormat_bc1_unorm_srgb: result = S("bc1_unorm_srgb"); break;
		case PixelFormat_bc2_typeless: result = S("bc2_typeless"); break;
		case PixelFormat_bc2_unorm: result = S("bc2_unorm"); break;
		case PixelFormat_bc2_unorm_srgb: result = S("bc2_unorm_srgb"); break;
		case PixelFormat_bc3_typeless: result = S("bc3_typeless"); break;
		case PixelFormat_bc3_unorm: result = S("bc3_unorm"); break;
		case PixelFormat_bc3_unorm_srgb: result = S("bc3_unorm_srgb"); break;
		case PixelFormat_bc4_typeless: result = S("bc4_typeless"); break;
		case PixelFormat_bc4_unorm: result = S("bc4_unorm"); break;
		case PixelFormat_bc4_snorm: result = S("bc4_snorm"); break;
		case PixelFormat_bc5_typeless: result = S("bc5_typeless"); break;
		case PixelFormat_bc5_unorm: result = S("bc5_unorm"); break;
		case PixelFormat_bc5_snorm: result = S("bc5_snorm"); break;
		case PixelFormat_b5g6r5_unorm: result = S("b5g6r5_unorm"); break;
		case PixelFormat_b5g5r5a1_unorm: result = S("b5g5r5a1_unorm"); break;
		case PixelFormat_b8g8r8a8_unorm: result = S("b8g8r8a8_unorm"); break;
		case PixelFormat_b8g8r8x8_unorm: result = S("b8g8r8x8_unorm"); break;
		case PixelFormat_r10g10b10_xr_bias_a2_unorm: result = S("r10g10b10_xr_bias_a2_unorm"); break;
		case PixelFormat_b8g8r8a8_typeless: result = S("b8g8r8a8_typeless"); break;
		case PixelFormat_b8g8r8a8_unorm_srgb: result = S("b8g8r8a8_unorm_srgb"); break;
		case PixelFormat_b8g8r8x8_typeless: result = S("b8g8r8x8_typeless"); break;
		case PixelFormat_b8g8r8x8_unorm_srgb: result = S("b8g8r8x8_unorm_srgb"); break;
		case PixelFormat_bc6h_typeless: result = S("bc6h_typeless"); break;
		case PixelFormat_bc6h_uf16: result = S("bc6h_uf16"); break;
		case PixelFormat_bc6h_sf16: result = S("bc6h_sf16"); break;
		case PixelFormat_bc7_typeless: result = S("bc7_typeless"); break;
		case PixelFormat_bc7_unorm: result = S("bc7_unorm"); break;
		case PixelFormat_bc7_unorm_srgb: result = S("bc7_unorm_srgb"); break;
		case PixelFormat_ayuv: result = S("ayuv"); break;
		case PixelFormat_y410: result = S("y410"); break;
		case PixelFormat_y416: result = S("y416"); break;
		case PixelFormat_nv12: result = S("nv12"); break;
		case PixelFormat_p010: result = S("p010"); break;
		case PixelFormat_p016: result = S("p016"); break;
		case PixelFormat_420_opaque: result = S("420_opaque"); break;
		case PixelFormat_yuy2: result = S("yuy2"); break;
		case PixelFormat_y210: result = S("y210"); break;
		case PixelFormat_y216: result = S("y216"); break;
		case PixelFormat_nv11: result = S("nv11"); break;
		case PixelFormat_ai44: result = S("ai44"); break;
		case PixelFormat_ia44: result = S("ia44"); break;
		case PixelFormat_p8: result = S("p8"); break;
		case PixelFormat_a8p8: result = S("a8p8"); break;
		case PixelFormat_b4g4r4a4_unorm: result = S("b4g4r4a4_unorm"); break;
		case PixelFormat_p208: result = S("p208"); break;
		case PixelFormat_v208: result = S("v208"); break;
		case PixelFormat_v408: result = S("v408"); break;
		case PixelFormat_sampler_feedback_min_mip_opaque:result = S("sampler_feedback_min_mip_opaque"); break;
		case PixelFormat_sampler_feedback_mip_region_used_opaque:result = S("sampler_feedback_mip_region_used_opaque"); break;
	}

	return result;
}