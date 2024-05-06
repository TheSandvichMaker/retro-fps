//
// rhi -> d3d12
//

D3D12_FILL_MODE to_d3d12_fill_mode(rhi_fill_mode_t mode)
{
	static D3D12_FILL_MODE map[] = {
		[RhiFillMode_solid]     = D3D12_FILL_MODE_SOLID,
		[RhiFillMode_wireframe] = D3D12_FILL_MODE_WIREFRAME,
	};

	ASSERT(mode >= 0 && mode < ARRAY_COUNT(map));
	return map[mode];
}

D3D12_CULL_MODE to_d3d12_cull_mode(rhi_cull_mode_t mode)
{
	static D3D12_CULL_MODE map[] = {
		[RhiCullMode_none]  = D3D12_CULL_MODE_NONE,
		[RhiCullMode_back]  = D3D12_CULL_MODE_FRONT,
		[RhiCullMode_front] = D3D12_CULL_MODE_BACK,
	};

	ASSERT(mode >= 0 && mode < ARRAY_COUNT(map));
	return map[mode];
}

D3D12_COMPARISON_FUNC to_d3d12_comparison_func(rhi_comparison_func_t func)
{
	static D3D12_COMPARISON_FUNC map[] = {
		[RhiComparisonFunc_none]          = 0,
		[RhiComparisonFunc_never]         = D3D12_COMPARISON_FUNC_NEVER,
		[RhiComparisonFunc_less]          = D3D12_COMPARISON_FUNC_LESS,
		[RhiComparisonFunc_equal]         = D3D12_COMPARISON_FUNC_EQUAL,
		[RhiComparisonFunc_less_equal]    = D3D12_COMPARISON_FUNC_LESS_EQUAL,
		[RhiComparisonFunc_greater]       = D3D12_COMPARISON_FUNC_GREATER,
		[RhiComparisonFunc_not_equal]     = D3D12_COMPARISON_FUNC_NOT_EQUAL,
		[RhiComparisonFunc_greater_equal] = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
		[RhiComparisonFunc_always]        = D3D12_COMPARISON_FUNC_ALWAYS,
	};

	ASSERT(func >= 0 && func < ARRAY_COUNT(map));
	return map[func];
}

D3D12_BLEND to_d3d12_blend(rhi_blend_t blend)
{
	static D3D12_BLEND map[] = {
		[RhiBlend_none]             = 0,
		[RhiBlend_zero]             = D3D12_BLEND_ZERO,
		[RhiBlend_one]              = D3D12_BLEND_ONE,
		[RhiBlend_src_color]        = D3D12_BLEND_SRC_COLOR,
		[RhiBlend_inv_src_color]    = D3D12_BLEND_INV_SRC_COLOR,
		[RhiBlend_src_alpha]        = D3D12_BLEND_SRC_ALPHA,
		[RhiBlend_inv_src_alpha]    = D3D12_BLEND_INV_SRC_ALPHA,
		[RhiBlend_dest_alpha]       = D3D12_BLEND_DEST_ALPHA,
		[RhiBlend_inv_dest_alpha]   = D3D12_BLEND_INV_DEST_ALPHA,
		[RhiBlend_dest_color]       = D3D12_BLEND_DEST_COLOR,
		[RhiBlend_inv_dest_color]   = D3D12_BLEND_INV_DEST_COLOR,
		[RhiBlend_src_alpha_sat]    = D3D12_BLEND_SRC_ALPHA_SAT,
		[RhiBlend_blend_factor]     = D3D12_BLEND_BLEND_FACTOR,
		[RhiBlend_inv_blend_factor] = D3D12_BLEND_INV_BLEND_FACTOR,
		[RhiBlend_src1_color]       = D3D12_BLEND_SRC1_COLOR,
		[RhiBlend_inv_src1_color]   = D3D12_BLEND_INV_SRC1_COLOR,
		[RhiBlend_src1_alpha]       = D3D12_BLEND_SRC1_ALPHA,
		[RhiBlend_inv_src1_alpha]   = D3D12_BLEND_INV_SRC1_ALPHA,
		[RhiBlend_alpha_factor]     = D3D12_BLEND_ALPHA_FACTOR,
		[RhiBlend_inv_alpha_factor] = D3D12_BLEND_INV_ALPHA_FACTOR,
	};

	ASSERT(blend >= 0 && blend < ARRAY_COUNT(map));
	return map[blend];
}

D3D12_BLEND_OP to_d3d12_blend_op(rhi_blend_op_t op)
{
	static D3D12_BLEND_OP map[] = {
		[RhiBlendOp_none]         = 0,
		[RhiBlendOp_add]          = D3D12_BLEND_OP_ADD,
		[RhiBlendOp_subtract]     = D3D12_BLEND_OP_SUBTRACT,
		[RhiBlendOp_rev_subtract] = D3D12_BLEND_OP_REV_SUBTRACT,
		[RhiBlendOp_min]          = D3D12_BLEND_OP_MIN,
		[RhiBlendOp_max]          = D3D12_BLEND_OP_MAX,
	};

	ASSERT(op >= 0 && op < ARRAY_COUNT(map));
	return map[op];
}

D3D12_LOGIC_OP to_d3d12_logic_op(rhi_logic_op_t op)
{
	static D3D12_LOGIC_OP map[] = {
		[RhiLogicOp_clear]         = D3D12_LOGIC_OP_CLEAR,
		[RhiLogicOp_set]           = D3D12_LOGIC_OP_SET,
		[RhiLogicOp_copy]          = D3D12_LOGIC_OP_COPY,
		[RhiLogicOp_copy_inverted] = D3D12_LOGIC_OP_COPY_INVERTED,
		[RhiLogicOp_noop]          = D3D12_LOGIC_OP_NOOP,
		[RhiLogicOp_invert]        = D3D12_LOGIC_OP_INVERT,
		[RhiLogicOp_and]           = D3D12_LOGIC_OP_AND,
		[RhiLogicOp_nand]          = D3D12_LOGIC_OP_NAND,
		[RhiLogicOp_or]            = D3D12_LOGIC_OP_OR,
		[RhiLogicOp_nor]           = D3D12_LOGIC_OP_NOR,
		[RhiLogicOp_xor]           = D3D12_LOGIC_OP_XOR,
		[RhiLogicOp_equiv]         = D3D12_LOGIC_OP_EQUIV,
		[RhiLogicOp_and_reverse]   = D3D12_LOGIC_OP_AND_REVERSE,
		[RhiLogicOp_and_inverted]  = D3D12_LOGIC_OP_AND_INVERTED,
		[RhiLogicOp_or_reverse]    = D3D12_LOGIC_OP_OR_REVERSE,
		[RhiLogicOp_or_inverted]   = D3D12_LOGIC_OP_OR_INVERTED,
	};

	ASSERT(op >= 0 && op < ARRAY_COUNT(map));
	return map[op];
}

fn D3D12_PRIMITIVE_TOPOLOGY to_d3d12_primitive_topology(rhi_primitive_topology_t topology)
{
	static D3D12_PRIMITIVE_TOPOLOGY map[] = {
		[RhiPrimitiveTopology_undefined]         = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
		[RhiPrimitiveTopology_pointlist]         = D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
		[RhiPrimitiveTopology_linelist]          = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
		[RhiPrimitiveTopology_linestrip]         = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
		[RhiPrimitiveTopology_trianglelist]      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		[RhiPrimitiveTopology_trianglestrip]     = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
		[RhiPrimitiveTopology_linelist_adj]      = D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
		[RhiPrimitiveTopology_linestrip_adj]     = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
		[RhiPrimitiveTopology_trianglelist_adj]  = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
		[RhiPrimitiveTopology_trianglestrip_adj] = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,
	};

	ASSERT(topology >= 0 && topology < ARRAY_COUNT(map));
	return map[topology];
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE to_d3d12_primitive_topology_type(rhi_primitive_topology_type_t topology)
{
	static D3D12_PRIMITIVE_TOPOLOGY_TYPE map[] = {
		[RhiPrimitiveTopologyType_undefined] = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
		[RhiPrimitiveTopologyType_point]     = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		[RhiPrimitiveTopologyType_line]      = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
		[RhiPrimitiveTopologyType_triangle]  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		[RhiPrimitiveTopologyType_patch]     = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
	};

	ASSERT(topology >= 0 && topology < ARRAY_COUNT(map));
	return map[topology];
}

D3D12_STENCIL_OP to_d3d12_stencil_op(rhi_stencil_op_t op)
{
	static D3D12_STENCIL_OP map[] = {
		[RhiStencilOp_none]     = 0,
		[RhiStencilOp_keep]     = D3D12_STENCIL_OP_KEEP,
		[RhiStencilOp_zero]     = D3D12_STENCIL_OP_ZERO,
		[RhiStencilOp_replace]  = D3D12_STENCIL_OP_REPLACE,
		[RhiStencilOp_incr_sat] = D3D12_STENCIL_OP_INCR_SAT,
		[RhiStencilOp_decr_sat] = D3D12_STENCIL_OP_DECR_SAT,
		[RhiStencilOp_invert]   = D3D12_STENCIL_OP_INVERT,
		[RhiStencilOp_incr]     = D3D12_STENCIL_OP_INCR,
		[RhiStencilOp_decr]     = D3D12_STENCIL_OP_DECR,
	};

	ASSERT(op >= 0 && op < ARRAY_COUNT(map));
	return map[op];
}

DXGI_FORMAT to_dxgi_format(rhi_pixel_format_t pf)
{
	static DXGI_FORMAT map[] = {
		[RhiPixelFormat_unknown]                                 = DXGI_FORMAT_UNKNOWN,
		[RhiPixelFormat_r32g32b32a32_typeless]                   = DXGI_FORMAT_R32G32B32A32_TYPELESS,
		[RhiPixelFormat_r32g32b32a32_float]                      = DXGI_FORMAT_R32G32B32A32_FLOAT,
		[RhiPixelFormat_r32g32b32a32_uint]                       = DXGI_FORMAT_R32G32B32A32_UINT,
		[RhiPixelFormat_r32g32b32a32_sint]                       = DXGI_FORMAT_R32G32B32A32_SINT,
		[RhiPixelFormat_r32g32b32_typeless]                      = DXGI_FORMAT_R32G32B32_TYPELESS,
		[RhiPixelFormat_r32g32b32_float]                         = DXGI_FORMAT_R32G32B32_FLOAT,
		[RhiPixelFormat_r32g32b32_uint]                          = DXGI_FORMAT_R32G32B32_UINT,
		[RhiPixelFormat_r32g32b32_sint]                          = DXGI_FORMAT_R32G32B32_SINT,
		[RhiPixelFormat_r16g16b16a16_typeless]                   = DXGI_FORMAT_R16G16B16A16_TYPELESS,
		[RhiPixelFormat_r16g16b16a16_float]                      = DXGI_FORMAT_R16G16B16A16_FLOAT,
		[RhiPixelFormat_r16g16b16a16_unorm]                      = DXGI_FORMAT_R16G16B16A16_UNORM,
		[RhiPixelFormat_r16g16b16a16_uint]                       = DXGI_FORMAT_R16G16B16A16_UINT,
		[RhiPixelFormat_r16g16b16a16_snorm]                      = DXGI_FORMAT_R16G16B16A16_SNORM,
		[RhiPixelFormat_r16g16b16a16_sint]                       = DXGI_FORMAT_R16G16B16A16_SINT,
		[RhiPixelFormat_r32g32_typeless]                         = DXGI_FORMAT_R32G32_TYPELESS,
		[RhiPixelFormat_r32g32_float]                            = DXGI_FORMAT_R32G32_FLOAT,
		[RhiPixelFormat_r32g32_uint]                             = DXGI_FORMAT_R32G32_UINT,
		[RhiPixelFormat_r32g32_sint]                             = DXGI_FORMAT_R32G32_SINT,
		[RhiPixelFormat_r32g8x24_typeless]                       = DXGI_FORMAT_R32G8X24_TYPELESS,
		[RhiPixelFormat_d32_float_s8x24_uint]                    = DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
		[RhiPixelFormat_r32_float_x8x24_typeless]                = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
		[RhiPixelFormat_x32_typeless_g8x24_uint]                 = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
		[RhiPixelFormat_r10g10b10a2_typeless]                    = DXGI_FORMAT_R10G10B10A2_TYPELESS,
		[RhiPixelFormat_r10g10b10a2_unorm]                       = DXGI_FORMAT_R10G10B10A2_UNORM,
		[RhiPixelFormat_r10g10b10a2_uint]                        = DXGI_FORMAT_R10G10B10A2_UINT,
		[RhiPixelFormat_r11g11b10_float]                         = DXGI_FORMAT_R11G11B10_FLOAT,
		[RhiPixelFormat_r8g8b8a8_typeless]                       = DXGI_FORMAT_R8G8B8A8_TYPELESS,
		[RhiPixelFormat_r8g8b8a8_unorm]                          = DXGI_FORMAT_R8G8B8A8_UNORM,
		[RhiPixelFormat_r8g8b8a8_unorm_srgb]                     = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		[RhiPixelFormat_r8g8b8a8_uint]                           = DXGI_FORMAT_R8G8B8A8_UINT,
		[RhiPixelFormat_r8g8b8a8_snorm]                          = DXGI_FORMAT_R8G8B8A8_SNORM,
		[RhiPixelFormat_r8g8b8a8_sint]                           = DXGI_FORMAT_R8G8B8A8_SINT,
		[RhiPixelFormat_r16g16_typeless]                         = DXGI_FORMAT_R16G16_TYPELESS,
		[RhiPixelFormat_r16g16_float]                            = DXGI_FORMAT_R16G16_FLOAT,
		[RhiPixelFormat_r16g16_unorm]                            = DXGI_FORMAT_R16G16_UNORM,
		[RhiPixelFormat_r16g16_uint]                             = DXGI_FORMAT_R16G16_UINT,
		[RhiPixelFormat_r16g16_snorm]                            = DXGI_FORMAT_R16G16_SNORM,
		[RhiPixelFormat_r16g16_sint]                             = DXGI_FORMAT_R16G16_SINT,
		[RhiPixelFormat_r32_typeless]                            = DXGI_FORMAT_R32_TYPELESS,
		[RhiPixelFormat_d32_float]                               = DXGI_FORMAT_D32_FLOAT,
		[RhiPixelFormat_r32_float]                               = DXGI_FORMAT_R32_FLOAT,
		[RhiPixelFormat_r32_uint]                                = DXGI_FORMAT_R32_UINT,
		[RhiPixelFormat_r32_sint]                                = DXGI_FORMAT_R32_SINT,
		[RhiPixelFormat_r24g8_typeless]                          = DXGI_FORMAT_R24G8_TYPELESS,
		[RhiPixelFormat_d24_unorm_s8_uint]                       = DXGI_FORMAT_D24_UNORM_S8_UINT,
		[RhiPixelFormat_r24_unorm_x8_typeless]                   = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
		[RhiPixelFormat_x24_typeless_g8_uint]                    = DXGI_FORMAT_X24_TYPELESS_G8_UINT,
		[RhiPixelFormat_r8g8_typeless]                           = DXGI_FORMAT_R8G8_TYPELESS,
		[RhiPixelFormat_r8g8_unorm]                              = DXGI_FORMAT_R8G8_UNORM,
		[RhiPixelFormat_r8g8_uint]                               = DXGI_FORMAT_R8G8_UINT,
		[RhiPixelFormat_r8g8_snorm]                              = DXGI_FORMAT_R8G8_SNORM,
		[RhiPixelFormat_r8g8_sint]                               = DXGI_FORMAT_R8G8_SINT,
		[RhiPixelFormat_r16_typeless]                            = DXGI_FORMAT_R16_TYPELESS,
		[RhiPixelFormat_r16_float]                               = DXGI_FORMAT_R16_FLOAT,
		[RhiPixelFormat_d16_unorm]                               = DXGI_FORMAT_D16_UNORM,
		[RhiPixelFormat_r16_unorm]                               = DXGI_FORMAT_R16_UNORM,
		[RhiPixelFormat_r16_uint]                                = DXGI_FORMAT_R16_UINT,
		[RhiPixelFormat_r16_snorm]                               = DXGI_FORMAT_R16_SNORM,
		[RhiPixelFormat_r16_sint]                                = DXGI_FORMAT_R16_SINT,
		[RhiPixelFormat_r8_typeless]                             = DXGI_FORMAT_R8_TYPELESS,
		[RhiPixelFormat_r8_unorm]                                = DXGI_FORMAT_R8_UNORM,
		[RhiPixelFormat_r8_uint]                                 = DXGI_FORMAT_R8_UINT,
		[RhiPixelFormat_r8_snorm]                                = DXGI_FORMAT_R8_SNORM,
		[RhiPixelFormat_r8_sint]                                 = DXGI_FORMAT_R8_SINT,
		[RhiPixelFormat_a8_unorm]                                = DXGI_FORMAT_A8_UNORM,
		[RhiPixelFormat_r1_unorm]                                = DXGI_FORMAT_R1_UNORM,
		[RhiPixelFormat_r9g9b9e5_sharedexp]                      = DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
		[RhiPixelFormat_r8g8_b8g8_unorm]                         = DXGI_FORMAT_R8G8_B8G8_UNORM,
		[RhiPixelFormat_g8r8_g8b8_unorm]                         = DXGI_FORMAT_G8R8_G8B8_UNORM,
		[RhiPixelFormat_bc1_typeless]                            = DXGI_FORMAT_BC1_TYPELESS,
		[RhiPixelFormat_bc1_unorm]                               = DXGI_FORMAT_BC1_UNORM,
		[RhiPixelFormat_bc1_unorm_srgb]                          = DXGI_FORMAT_BC1_UNORM_SRGB,
		[RhiPixelFormat_bc2_typeless]                            = DXGI_FORMAT_BC2_TYPELESS,
		[RhiPixelFormat_bc2_unorm]                               = DXGI_FORMAT_BC2_UNORM,
		[RhiPixelFormat_bc2_unorm_srgb]                          = DXGI_FORMAT_BC2_UNORM_SRGB,
		[RhiPixelFormat_bc3_typeless]                            = DXGI_FORMAT_BC3_TYPELESS,
		[RhiPixelFormat_bc3_unorm]                               = DXGI_FORMAT_BC3_UNORM,
		[RhiPixelFormat_bc3_unorm_srgb]                          = DXGI_FORMAT_BC3_UNORM_SRGB,
		[RhiPixelFormat_bc4_typeless]                            = DXGI_FORMAT_BC4_TYPELESS,
		[RhiPixelFormat_bc4_unorm]                               = DXGI_FORMAT_BC4_UNORM,
		[RhiPixelFormat_bc4_snorm]                               = DXGI_FORMAT_BC4_SNORM,
		[RhiPixelFormat_bc5_typeless]                            = DXGI_FORMAT_BC5_TYPELESS,
		[RhiPixelFormat_bc5_unorm]                               = DXGI_FORMAT_BC5_UNORM,
		[RhiPixelFormat_bc5_snorm]                               = DXGI_FORMAT_BC5_SNORM,
		[RhiPixelFormat_b5g6r5_unorm]                            = DXGI_FORMAT_B5G6R5_UNORM,
		[RhiPixelFormat_b5g5r5a1_unorm]                          = DXGI_FORMAT_B5G5R5A1_UNORM,
		[RhiPixelFormat_b8g8r8a8_unorm]                          = DXGI_FORMAT_B8G8R8A8_UNORM,
		[RhiPixelFormat_b8g8r8x8_unorm]                          = DXGI_FORMAT_B8G8R8X8_UNORM,
		[RhiPixelFormat_r10g10b10_xr_bias_a2_unorm]              = DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
		[RhiPixelFormat_b8g8r8a8_typeless]                       = DXGI_FORMAT_B8G8R8A8_TYPELESS,
		[RhiPixelFormat_b8g8r8a8_unorm_srgb]                     = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		[RhiPixelFormat_b8g8r8x8_typeless]                       = DXGI_FORMAT_B8G8R8X8_TYPELESS,
		[RhiPixelFormat_b8g8r8x8_unorm_srgb]                     = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
		[RhiPixelFormat_bc6h_typeless]                           = DXGI_FORMAT_BC6H_TYPELESS,
		[RhiPixelFormat_bc6h_uf16]                               = DXGI_FORMAT_BC6H_UF16,
		[RhiPixelFormat_bc6h_sf16]                               = DXGI_FORMAT_BC6H_SF16,
		[RhiPixelFormat_bc7_typeless]                            = DXGI_FORMAT_BC7_TYPELESS,
		[RhiPixelFormat_bc7_unorm]                               = DXGI_FORMAT_BC7_UNORM,
		[RhiPixelFormat_bc7_unorm_srgb]                          = DXGI_FORMAT_BC7_UNORM_SRGB,
		[RhiPixelFormat_ayuv]                                    = DXGI_FORMAT_AYUV,
		[RhiPixelFormat_y410]                                    = DXGI_FORMAT_Y410,
		[RhiPixelFormat_y416]                                    = DXGI_FORMAT_Y416,
		[RhiPixelFormat_nv12]                                    = DXGI_FORMAT_NV12,
		[RhiPixelFormat_p010]                                    = DXGI_FORMAT_P010,
		[RhiPixelFormat_p016]                                    = DXGI_FORMAT_P016,
		[RhiPixelFormat_420_opaque]                              = DXGI_FORMAT_420_OPAQUE,
		[RhiPixelFormat_yuy2]                                    = DXGI_FORMAT_YUY2,
		[RhiPixelFormat_y210]                                    = DXGI_FORMAT_Y210,
		[RhiPixelFormat_y216]                                    = DXGI_FORMAT_Y216,
		[RhiPixelFormat_nv11]                                    = DXGI_FORMAT_NV11,
		[RhiPixelFormat_ai44]                                    = DXGI_FORMAT_AI44,
		[RhiPixelFormat_ia44]                                    = DXGI_FORMAT_IA44,
		[RhiPixelFormat_p8]                                      = DXGI_FORMAT_P8,
		[RhiPixelFormat_a8p8]                                    = DXGI_FORMAT_A8P8,
		[RhiPixelFormat_b4g4r4a4_unorm]                          = DXGI_FORMAT_B4G4R4A4_UNORM,
		[RhiPixelFormat_p208]                                    = DXGI_FORMAT_P208,
		[RhiPixelFormat_v208]                                    = DXGI_FORMAT_V208,
		[RhiPixelFormat_v408]                                    = DXGI_FORMAT_V408,
		[RhiPixelFormat_sampler_feedback_min_mip_opaque]         = DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
		[RhiPixelFormat_sampler_feedback_mip_region_used_opaque] = DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
	};

	ASSERT(pf >= 0 && pf < ARRAY_COUNT(map));
	return map[pf];
}

//
// d3d12 -> rhi
//

rhi_fill_mode_t from_d3d12_fill_mode(D3D12_FILL_MODE mode)
{
	static rhi_fill_mode_t map[] = {
		[D3D12_FILL_MODE_SOLID]     = RhiFillMode_solid,
		[D3D12_FILL_MODE_WIREFRAME] = RhiFillMode_wireframe,
	};

	ASSERT(mode >= 0 && mode < ARRAY_COUNT(map));
	return map[mode];
}

rhi_cull_mode_t from_d3d12_cull_mode(D3D12_CULL_MODE mode)
{
	static rhi_cull_mode_t map[] = {
		[D3D12_CULL_MODE_NONE]  = RhiCullMode_none,
		[D3D12_CULL_MODE_FRONT] = RhiCullMode_back,
		[D3D12_CULL_MODE_BACK]  = RhiCullMode_front,
	};

	ASSERT(mode >= 0 && mode < ARRAY_COUNT(map));
	return map[mode];
}

rhi_comparison_func_t from_d3d12_comparison_func(D3D12_COMPARISON_FUNC func)
{
	static rhi_comparison_func_t map[] = {
		[0]                                   = RhiComparisonFunc_none,
		[D3D12_COMPARISON_FUNC_NEVER]         = RhiComparisonFunc_never,
		[D3D12_COMPARISON_FUNC_LESS]          = RhiComparisonFunc_less,
		[D3D12_COMPARISON_FUNC_EQUAL]         = RhiComparisonFunc_equal,
		[D3D12_COMPARISON_FUNC_LESS_EQUAL]    = RhiComparisonFunc_less_equal,
		[D3D12_COMPARISON_FUNC_GREATER]       = RhiComparisonFunc_greater,
		[D3D12_COMPARISON_FUNC_NOT_EQUAL]     = RhiComparisonFunc_not_equal,
		[D3D12_COMPARISON_FUNC_GREATER_EQUAL] = RhiComparisonFunc_greater_equal,
		[D3D12_COMPARISON_FUNC_ALWAYS]        = RhiComparisonFunc_always,
	};

	ASSERT(func >= 0 && func < ARRAY_COUNT(map));
	return map[func];
}

rhi_blend_t from_d3d12_blend(D3D12_BLEND blend)
{
	static rhi_blend_t map[] = {
		[0]                            = RhiBlend_none,
		[D3D12_BLEND_ZERO]             = RhiBlend_zero,
		[D3D12_BLEND_ONE]              = RhiBlend_one,
		[D3D12_BLEND_SRC_COLOR]        = RhiBlend_src_color,
		[D3D12_BLEND_INV_SRC_COLOR]    = RhiBlend_inv_src_color,
		[D3D12_BLEND_SRC_ALPHA]        = RhiBlend_src_alpha,
		[D3D12_BLEND_INV_SRC_ALPHA]    = RhiBlend_inv_src_alpha,
		[D3D12_BLEND_DEST_ALPHA]       = RhiBlend_dest_alpha,
		[D3D12_BLEND_INV_DEST_ALPHA]   = RhiBlend_inv_dest_alpha,
		[D3D12_BLEND_DEST_COLOR]       = RhiBlend_dest_color,
		[D3D12_BLEND_INV_DEST_COLOR]   = RhiBlend_inv_dest_color,
		[D3D12_BLEND_SRC_ALPHA_SAT]    = RhiBlend_src_alpha_sat,
		[D3D12_BLEND_BLEND_FACTOR]     = RhiBlend_blend_factor,
		[D3D12_BLEND_INV_BLEND_FACTOR] = RhiBlend_inv_blend_factor,
		[D3D12_BLEND_SRC1_COLOR]       = RhiBlend_src1_color,
		[D3D12_BLEND_INV_SRC1_COLOR]   = RhiBlend_inv_src1_color,
		[D3D12_BLEND_SRC1_ALPHA]       = RhiBlend_src1_alpha,
		[D3D12_BLEND_INV_SRC1_ALPHA]   = RhiBlend_inv_src1_alpha,
		[D3D12_BLEND_ALPHA_FACTOR]     = RhiBlend_alpha_factor,
		[D3D12_BLEND_INV_ALPHA_FACTOR] = RhiBlend_inv_alpha_factor,
	};

	ASSERT(blend >= 0 && blend < ARRAY_COUNT(map));
	return map[blend];
}

rhi_blend_op_t from_d3d12_blend_op(D3D12_BLEND_OP op)
{
	static rhi_blend_op_t map[] = {
		[0]                           = RhiBlendOp_none,
		[D3D12_BLEND_OP_ADD]          = RhiBlendOp_add,
		[D3D12_BLEND_OP_SUBTRACT]     = RhiBlendOp_subtract,
		[D3D12_BLEND_OP_REV_SUBTRACT] = RhiBlendOp_rev_subtract,
		[D3D12_BLEND_OP_MIN]          = RhiBlendOp_min,
		[D3D12_BLEND_OP_MAX]          = RhiBlendOp_max,
	};

	ASSERT(op >= 0 && op < ARRAY_COUNT(map));
	return map[op];
}

rhi_logic_op_t from_d3d12_logic_op(D3D12_LOGIC_OP op)
{
	static rhi_logic_op_t map[] = {
		[D3D12_LOGIC_OP_CLEAR]         = RhiLogicOp_clear,
		[D3D12_LOGIC_OP_SET]           = RhiLogicOp_set,
		[D3D12_LOGIC_OP_COPY]          = RhiLogicOp_copy,
		[D3D12_LOGIC_OP_COPY_INVERTED] = RhiLogicOp_copy_inverted,
		[D3D12_LOGIC_OP_NOOP]          = RhiLogicOp_noop,
		[D3D12_LOGIC_OP_INVERT]        = RhiLogicOp_invert,
		[D3D12_LOGIC_OP_AND]           = RhiLogicOp_and,
		[D3D12_LOGIC_OP_NAND]          = RhiLogicOp_nand,
		[D3D12_LOGIC_OP_OR]            = RhiLogicOp_or,
		[D3D12_LOGIC_OP_NOR]           = RhiLogicOp_nor,
		[D3D12_LOGIC_OP_XOR]           = RhiLogicOp_xor,
		[D3D12_LOGIC_OP_EQUIV]         = RhiLogicOp_equiv,
		[D3D12_LOGIC_OP_AND_REVERSE]   = RhiLogicOp_and_reverse,
		[D3D12_LOGIC_OP_AND_INVERTED]  = RhiLogicOp_and_inverted,
		[D3D12_LOGIC_OP_OR_REVERSE]    = RhiLogicOp_or_reverse,
		[D3D12_LOGIC_OP_OR_INVERTED]   = RhiLogicOp_or_inverted,
	};

	ASSERT(op >= 0 && op < ARRAY_COUNT(map));
	return map[op];
}

rhi_primitive_topology_t from_d3d12_primitive_topology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
	static rhi_primitive_topology_t map[] = {
		[D3D_PRIMITIVE_TOPOLOGY_UNDEFINED]         = RhiPrimitiveTopology_undefined,
		[D3D_PRIMITIVE_TOPOLOGY_POINTLIST]         = RhiPrimitiveTopology_pointlist,
		[D3D_PRIMITIVE_TOPOLOGY_LINELIST]          = RhiPrimitiveTopology_linelist,
		[D3D_PRIMITIVE_TOPOLOGY_LINESTRIP]         = RhiPrimitiveTopology_linestrip,
		[D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST]      = RhiPrimitiveTopology_trianglelist,
		[D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP]     = RhiPrimitiveTopology_trianglestrip,
		[D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ]      = RhiPrimitiveTopology_linelist_adj,
		[D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ]     = RhiPrimitiveTopology_linestrip_adj,
		[D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ]  = RhiPrimitiveTopology_trianglelist_adj,
		[D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ] = RhiPrimitiveTopology_trianglestrip_adj,
	};

	ASSERT(topology >= 0 && topology < ARRAY_COUNT(map));
	return map[topology];
}

rhi_primitive_topology_type_t from_d3d12_primitive_topology_type(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
{
	static rhi_primitive_topology_type_t map[] = {
		[D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED] = RhiPrimitiveTopologyType_undefined,
		[D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT]     = RhiPrimitiveTopologyType_point,
		[D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE]      = RhiPrimitiveTopologyType_line,
		[D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE]  = RhiPrimitiveTopologyType_triangle,
		[D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH]     = RhiPrimitiveTopologyType_patch,
	};

	ASSERT(topology >= 0 && topology < ARRAY_COUNT(map));
	return map[topology];
}

rhi_stencil_op_t from_d3d12_stencil_op(D3D12_STENCIL_OP op)
{
	static rhi_stencil_op_t map[] = {
		[0]     = RhiStencilOp_none,
		[D3D12_STENCIL_OP_KEEP]     = RhiStencilOp_keep,
		[D3D12_STENCIL_OP_ZERO]     = RhiStencilOp_zero,
		[D3D12_STENCIL_OP_REPLACE]  = RhiStencilOp_replace,
		[D3D12_STENCIL_OP_INCR_SAT] = RhiStencilOp_incr_sat,
		[D3D12_STENCIL_OP_DECR_SAT] = RhiStencilOp_decr_sat,
		[D3D12_STENCIL_OP_INVERT]   = RhiStencilOp_invert,
		[D3D12_STENCIL_OP_INCR]     = RhiStencilOp_incr,
		[D3D12_STENCIL_OP_DECR]     = RhiStencilOp_decr,
	};

	ASSERT(op >= 0 && op < ARRAY_COUNT(map));
	return map[op];
}

rhi_pixel_format_t from_dxgi_format(DXGI_FORMAT pf)
{
	static rhi_pixel_format_t map[] = {
		[DXGI_FORMAT_UNKNOWN]                                 = RhiPixelFormat_unknown,
		[DXGI_FORMAT_R32G32B32A32_TYPELESS]                   = RhiPixelFormat_r32g32b32a32_typeless,
		[DXGI_FORMAT_R32G32B32A32_FLOAT]                      = RhiPixelFormat_r32g32b32a32_float,
		[DXGI_FORMAT_R32G32B32A32_UINT]                       = RhiPixelFormat_r32g32b32a32_uint,
		[DXGI_FORMAT_R32G32B32A32_SINT]                       = RhiPixelFormat_r32g32b32a32_sint,
		[DXGI_FORMAT_R32G32B32_TYPELESS]                      = RhiPixelFormat_r32g32b32_typeless,
		[DXGI_FORMAT_R32G32B32_FLOAT]                         = RhiPixelFormat_r32g32b32_float,
		[DXGI_FORMAT_R32G32B32_UINT]                          = RhiPixelFormat_r32g32b32_uint,
		[DXGI_FORMAT_R32G32B32_SINT]                          = RhiPixelFormat_r32g32b32_sint,
		[DXGI_FORMAT_R16G16B16A16_TYPELESS]                   = RhiPixelFormat_r16g16b16a16_typeless,
		[DXGI_FORMAT_R16G16B16A16_FLOAT]                      = RhiPixelFormat_r16g16b16a16_float,
		[DXGI_FORMAT_R16G16B16A16_UNORM]                      = RhiPixelFormat_r16g16b16a16_unorm,
		[DXGI_FORMAT_R16G16B16A16_UINT]                       = RhiPixelFormat_r16g16b16a16_uint,
		[DXGI_FORMAT_R16G16B16A16_SNORM]                      = RhiPixelFormat_r16g16b16a16_snorm,
		[DXGI_FORMAT_R16G16B16A16_SINT]                       = RhiPixelFormat_r16g16b16a16_sint,
		[DXGI_FORMAT_R32G32_TYPELESS]                         = RhiPixelFormat_r32g32_typeless,
		[DXGI_FORMAT_R32G32_FLOAT]                            = RhiPixelFormat_r32g32_float,
		[DXGI_FORMAT_R32G32_UINT]                             = RhiPixelFormat_r32g32_uint,
		[DXGI_FORMAT_R32G32_SINT]                             = RhiPixelFormat_r32g32_sint,
		[DXGI_FORMAT_R32G8X24_TYPELESS]                       = RhiPixelFormat_r32g8x24_typeless,
		[DXGI_FORMAT_D32_FLOAT_S8X24_UINT]                    = RhiPixelFormat_d32_float_s8x24_uint,
		[DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS]                = RhiPixelFormat_r32_float_x8x24_typeless,
		[DXGI_FORMAT_X32_TYPELESS_G8X24_UINT]                 = RhiPixelFormat_x32_typeless_g8x24_uint,
		[DXGI_FORMAT_R10G10B10A2_TYPELESS]                    = RhiPixelFormat_r10g10b10a2_typeless,
		[DXGI_FORMAT_R10G10B10A2_UNORM]                       = RhiPixelFormat_r10g10b10a2_unorm,
		[DXGI_FORMAT_R10G10B10A2_UINT]                        = RhiPixelFormat_r10g10b10a2_uint,
		[DXGI_FORMAT_R11G11B10_FLOAT]                         = RhiPixelFormat_r11g11b10_float,
		[DXGI_FORMAT_R8G8B8A8_TYPELESS]                       = RhiPixelFormat_r8g8b8a8_typeless,
		[DXGI_FORMAT_R8G8B8A8_UNORM]                          = RhiPixelFormat_r8g8b8a8_unorm,
		[DXGI_FORMAT_R8G8B8A8_UNORM_SRGB]                     = RhiPixelFormat_r8g8b8a8_unorm_srgb,
		[DXGI_FORMAT_R8G8B8A8_UINT]                           = RhiPixelFormat_r8g8b8a8_uint,
		[DXGI_FORMAT_R8G8B8A8_SNORM]                          = RhiPixelFormat_r8g8b8a8_snorm,
		[DXGI_FORMAT_R8G8B8A8_SINT]                           = RhiPixelFormat_r8g8b8a8_sint,
		[DXGI_FORMAT_R16G16_TYPELESS]                         = RhiPixelFormat_r16g16_typeless,
		[DXGI_FORMAT_R16G16_FLOAT]                            = RhiPixelFormat_r16g16_float,
		[DXGI_FORMAT_R16G16_UNORM]                            = RhiPixelFormat_r16g16_unorm,
		[DXGI_FORMAT_R16G16_UINT]                             = RhiPixelFormat_r16g16_uint,
		[DXGI_FORMAT_R16G16_SNORM]                            = RhiPixelFormat_r16g16_snorm,
		[DXGI_FORMAT_R16G16_SINT]                             = RhiPixelFormat_r16g16_sint,
		[DXGI_FORMAT_R32_TYPELESS]                            = RhiPixelFormat_r32_typeless,
		[DXGI_FORMAT_D32_FLOAT]                               = RhiPixelFormat_d32_float,
		[DXGI_FORMAT_R32_FLOAT]                               = RhiPixelFormat_r32_float,
		[DXGI_FORMAT_R32_UINT]                                = RhiPixelFormat_r32_uint,
		[DXGI_FORMAT_R32_SINT]                                = RhiPixelFormat_r32_sint,
		[DXGI_FORMAT_R24G8_TYPELESS]                          = RhiPixelFormat_r24g8_typeless,
		[DXGI_FORMAT_D24_UNORM_S8_UINT]                       = RhiPixelFormat_d24_unorm_s8_uint,
		[DXGI_FORMAT_R24_UNORM_X8_TYPELESS]                   = RhiPixelFormat_r24_unorm_x8_typeless,
		[DXGI_FORMAT_X24_TYPELESS_G8_UINT]                    = RhiPixelFormat_x24_typeless_g8_uint,
		[DXGI_FORMAT_R8G8_TYPELESS]                           = RhiPixelFormat_r8g8_typeless,
		[DXGI_FORMAT_R8G8_UNORM]                              = RhiPixelFormat_r8g8_unorm,
		[DXGI_FORMAT_R8G8_UINT]                               = RhiPixelFormat_r8g8_uint,
		[DXGI_FORMAT_R8G8_SNORM]                              = RhiPixelFormat_r8g8_snorm,
		[DXGI_FORMAT_R8G8_SINT]                               = RhiPixelFormat_r8g8_sint,
		[DXGI_FORMAT_R16_TYPELESS]                            = RhiPixelFormat_r16_typeless,
		[DXGI_FORMAT_R16_FLOAT]                               = RhiPixelFormat_r16_float,
		[DXGI_FORMAT_D16_UNORM]                               = RhiPixelFormat_d16_unorm,
		[DXGI_FORMAT_R16_UNORM]                               = RhiPixelFormat_r16_unorm,
		[DXGI_FORMAT_R16_UINT]                                = RhiPixelFormat_r16_uint,
		[DXGI_FORMAT_R16_SNORM]                               = RhiPixelFormat_r16_snorm,
		[DXGI_FORMAT_R16_SINT]                                = RhiPixelFormat_r16_sint,
		[DXGI_FORMAT_R8_TYPELESS]                             = RhiPixelFormat_r8_typeless,
		[DXGI_FORMAT_R8_UNORM]                                = RhiPixelFormat_r8_unorm,
		[DXGI_FORMAT_R8_UINT]                                 = RhiPixelFormat_r8_uint,
		[DXGI_FORMAT_R8_SNORM]                                = RhiPixelFormat_r8_snorm,
		[DXGI_FORMAT_R8_SINT]                                 = RhiPixelFormat_r8_sint,
		[DXGI_FORMAT_A8_UNORM]                                = RhiPixelFormat_a8_unorm,
		[DXGI_FORMAT_R1_UNORM]                                = RhiPixelFormat_r1_unorm,
		[DXGI_FORMAT_R9G9B9E5_SHAREDEXP]                      = RhiPixelFormat_r9g9b9e5_sharedexp,
		[DXGI_FORMAT_R8G8_B8G8_UNORM]                         = RhiPixelFormat_r8g8_b8g8_unorm,
		[DXGI_FORMAT_G8R8_G8B8_UNORM]                         = RhiPixelFormat_g8r8_g8b8_unorm,
		[DXGI_FORMAT_BC1_TYPELESS]                            = RhiPixelFormat_bc1_typeless,
		[DXGI_FORMAT_BC1_UNORM]                               = RhiPixelFormat_bc1_unorm,
		[DXGI_FORMAT_BC1_UNORM_SRGB]                          = RhiPixelFormat_bc1_unorm_srgb,
		[DXGI_FORMAT_BC2_TYPELESS]                            = RhiPixelFormat_bc2_typeless,
		[DXGI_FORMAT_BC2_UNORM]                               = RhiPixelFormat_bc2_unorm,
		[DXGI_FORMAT_BC2_UNORM_SRGB]                          = RhiPixelFormat_bc2_unorm_srgb,
		[DXGI_FORMAT_BC3_TYPELESS]                            = RhiPixelFormat_bc3_typeless,
		[DXGI_FORMAT_BC3_UNORM]                               = RhiPixelFormat_bc3_unorm,
		[DXGI_FORMAT_BC3_UNORM_SRGB]                          = RhiPixelFormat_bc3_unorm_srgb,
		[DXGI_FORMAT_BC4_TYPELESS]                            = RhiPixelFormat_bc4_typeless,
		[DXGI_FORMAT_BC4_UNORM]                               = RhiPixelFormat_bc4_unorm,
		[DXGI_FORMAT_BC4_SNORM]                               = RhiPixelFormat_bc4_snorm,
		[DXGI_FORMAT_BC5_TYPELESS]                            = RhiPixelFormat_bc5_typeless,
		[DXGI_FORMAT_BC5_UNORM]                               = RhiPixelFormat_bc5_unorm,
		[DXGI_FORMAT_BC5_SNORM]                               = RhiPixelFormat_bc5_snorm,
		[DXGI_FORMAT_B5G6R5_UNORM]                            = RhiPixelFormat_b5g6r5_unorm,
		[DXGI_FORMAT_B5G5R5A1_UNORM]                          = RhiPixelFormat_b5g5r5a1_unorm,
		[DXGI_FORMAT_B8G8R8A8_UNORM]                          = RhiPixelFormat_b8g8r8a8_unorm,
		[DXGI_FORMAT_B8G8R8X8_UNORM]                          = RhiPixelFormat_b8g8r8x8_unorm,
		[DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM]              = RhiPixelFormat_r10g10b10_xr_bias_a2_unorm,
		[DXGI_FORMAT_B8G8R8A8_TYPELESS]                       = RhiPixelFormat_b8g8r8a8_typeless,
		[DXGI_FORMAT_B8G8R8A8_UNORM_SRGB]                     = RhiPixelFormat_b8g8r8a8_unorm_srgb,
		[DXGI_FORMAT_B8G8R8X8_TYPELESS]                       = RhiPixelFormat_b8g8r8x8_typeless,
		[DXGI_FORMAT_B8G8R8X8_UNORM_SRGB]                     = RhiPixelFormat_b8g8r8x8_unorm_srgb,
		[DXGI_FORMAT_BC6H_TYPELESS]                           = RhiPixelFormat_bc6h_typeless,
		[DXGI_FORMAT_BC6H_UF16]                               = RhiPixelFormat_bc6h_uf16,
		[DXGI_FORMAT_BC6H_SF16]                               = RhiPixelFormat_bc6h_sf16,
		[DXGI_FORMAT_BC7_TYPELESS]                            = RhiPixelFormat_bc7_typeless,
		[DXGI_FORMAT_BC7_UNORM]                               = RhiPixelFormat_bc7_unorm,
		[DXGI_FORMAT_BC7_UNORM_SRGB]                          = RhiPixelFormat_bc7_unorm_srgb,
		[DXGI_FORMAT_AYUV]                                    = RhiPixelFormat_ayuv,
		[DXGI_FORMAT_Y410]                                    = RhiPixelFormat_y410,
		[DXGI_FORMAT_Y416]                                    = RhiPixelFormat_y416,
		[DXGI_FORMAT_NV12]                                    = RhiPixelFormat_nv12,
		[DXGI_FORMAT_P010]                                    = RhiPixelFormat_p010,
		[DXGI_FORMAT_P016]                                    = RhiPixelFormat_p016,
		[DXGI_FORMAT_420_OPAQUE]                              = RhiPixelFormat_420_opaque,
		[DXGI_FORMAT_YUY2]                                    = RhiPixelFormat_yuy2,
		[DXGI_FORMAT_Y210]                                    = RhiPixelFormat_y210,
		[DXGI_FORMAT_Y216]                                    = RhiPixelFormat_y216,
		[DXGI_FORMAT_NV11]                                    = RhiPixelFormat_nv11,
		[DXGI_FORMAT_AI44]                                    = RhiPixelFormat_ai44,
		[DXGI_FORMAT_IA44]                                    = RhiPixelFormat_ia44,
		[DXGI_FORMAT_P8]                                      = RhiPixelFormat_p8,
		[DXGI_FORMAT_A8P8]                                    = RhiPixelFormat_a8p8,
		[DXGI_FORMAT_B4G4R4A4_UNORM]                          = RhiPixelFormat_b4g4r4a4_unorm,
		[DXGI_FORMAT_P208]                                    = RhiPixelFormat_p208,
		[DXGI_FORMAT_V208]                                    = RhiPixelFormat_v208,
		[DXGI_FORMAT_V408]                                    = RhiPixelFormat_v408,
		[DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE]         = RhiPixelFormat_sampler_feedback_min_mip_opaque,
		[DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE] = RhiPixelFormat_sampler_feedback_mip_region_used_opaque,
	};

	ASSERT(pf >= 0 && pf < ARRAY_COUNT(map));
	return map[pf];
}
