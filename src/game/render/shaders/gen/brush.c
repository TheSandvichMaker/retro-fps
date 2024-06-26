// Generated by metagen.lua

void shader_brush_set_draw_params(rhi_command_list_t *list, brush_draw_parameters_t *params)
{
	rhi_validate_texture_srv(params->albedo, S("shader_brush_set_draw_params"));
	rhi_validate_texture_srv(params->lightmap, S("shader_brush_set_draw_params"));
	rhi_set_parameters(list, 0, params, sizeof(*params));
}

void shader_brush_set_pass_params(rhi_command_list_t *list, brush_pass_parameters_t *params)
{
	rhi_validate_buffer_srv(params->lm_uvs, S("shader_brush_set_pass_params"));
	rhi_validate_buffer_srv(params->positions, S("shader_brush_set_pass_params"));
	rhi_validate_texture_srv(params->sun_shadowmap, S("shader_brush_set_pass_params"));
	rhi_validate_buffer_srv(params->uvs, S("shader_brush_set_pass_params"));
	rhi_set_parameters(list, 1, params, sizeof(*params));
}

#define DF_SHADER_BRUSH_SOURCE_CODE \
	"// Generated by metagen.lua\n" \
	"\n" \
	"#include \"bindless.hlsli\"\n" \
	"\n" \
	"#include \"common.hlsli\"\n" \
	"\n" \
	"struct brush_draw_parameters_t\n" \
	"{\n" \
	"	df::Resource< Texture2D< float3 > > albedo;\n" \
	"	float3 normal;\n" \
	"	df::Resource< Texture2D< float3 > > lightmap;\n" \
	"};\n" \
	"\n" \
	"ConstantBuffer< brush_draw_parameters_t > draw : register(b0);\n" \
	"\n" \
	"struct brush_pass_parameters_t\n" \
	"{\n" \
	"	df::Resource< StructuredBuffer< float2 > > lm_uvs;\n" \
	"	uint3 pad0;\n" \
	"	df::Resource< StructuredBuffer< float3 > > positions;\n" \
	"	uint3 pad1;\n" \
	"	df::Resource< Texture2D< float > > sun_shadowmap;\n" \
	"	uint3 pad2;\n" \
	"	df::Resource< StructuredBuffer< float2 > > uvs;\n" \
	"};\n" \
	"\n" \
	"ConstantBuffer< brush_pass_parameters_t > pass : register(b1);\n" \
	"\n" \
	"\n" \
	"\n" \
	"struct VS_OUT\n" \
	"{\n" \
	"	float4 position           : SV_Position;\n" \
	"	float4 shadowmap_position : SHADOWMAP_POSITION;\n" \
	"	float2 uv                 : TEXCOORD;\n" \
	"	float2 lightmap_uv        : LIGHTMAP_TEXCOORD;\n" \
	"};\n" \
	"\n" \
	"VS_OUT brush_vs(uint vertex_index : SV_VertexID)\n" \
	"{\n" \
	"	float3 position    = pass.positions.Get().Load(vertex_index);\n" \
	"	float2 uv          = pass.uvs      .Get().Load(vertex_index);\n" \
	"	float2 lightmap_uv = pass.lm_uvs   .Get().Load(vertex_index);\n" \
	"\n" \
	"	VS_OUT OUT;\n" \
	"	OUT.position           = mul(view.world_to_clip, float4(position, 1));\n" \
	"	OUT.shadowmap_position = mul(view.sun_matrix,    float4(position, 1));\n" \
	"	OUT.uv                 = uv;\n" \
	"	OUT.lightmap_uv        = lightmap_uv;\n" \
	"	return OUT;\n" \
	"}\n" \
	"\n" \
	"float4 brush_ps(VS_OUT IN) : SV_Target\n" \
	"{\n" \
	"	Texture2D<float3> albedo = draw.albedo.Get();\n" \
	"\n" \
	"	float2 albedo_dim;\n" \
	"	albedo.GetDimensions(albedo_dim.x, albedo_dim.y);\n" \
	"\n" \
	"	float3 color = albedo.Sample(df::s_aniso_wrap, FatPixel(albedo_dim, IN.uv)).rgb;\n" \
	"\n" \
	"	Texture2D<float3> lightmap = draw.lightmap.Get();\n" \
	"\n" \
	"	float2 lightmap_dim;\n" \
	"	lightmap.GetDimensions(lightmap_dim.x, lightmap_dim.y);\n" \
	"\n" \
	"	float3 light = lightmap.Sample(df::s_aniso_clamped, IN.lightmap_uv/*FatPixel(lightmap_dim, IN.lightmap_uv)*/).rgb;\n" \
	"\n" \
	"	const float3 shadow_test_pos   = IN.shadowmap_position.xyz / IN.shadowmap_position.w;\n" \
	"	const float2 shadow_test_uv    = 0.5 + float2(0.5, -0.5)*shadow_test_pos.xy;\n" \
	"	const float  shadow_test_depth = shadow_test_pos.z;\n" \
	"\n" \
	"	float shadow_factor = 1.0;\n" \
	"\n" \
	"	if (shadow_test_depth >= 0.0 && shadow_test_depth <= 1.0)\n" \
	"	{\n" \
	"		const float bias = max(0.025*(1.0 - max(0, dot(draw.normal, view.sun_direction))), 0.010);\n" \
	"		const float biased_depth = shadow_test_depth + bias;\n" \
	"\n" \
	"		float2 shadowmap_dim;\n" \
	"		pass.sun_shadowmap.Get().GetDimensions(shadowmap_dim.x, shadowmap_dim.y);\n" \
	"\n" \
	"		shadow_factor = SampleShadowPCF3x3(pass.sun_shadowmap.Get(), shadowmap_dim, shadow_test_uv, biased_depth);\n" \
	"	}\n" \
	"\n" \
	"	light += shadow_factor*(view.sun_color*saturate(dot(draw.normal, view.sun_direction)));\n" \
	"\n" \
	"	return float4(color*light, 1);\n" \
	"}\n" \
	"\n" \
