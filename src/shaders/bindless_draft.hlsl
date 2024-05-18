#include "common.hlsli"

struct PassParameters
{
	df::Resource< StructuredBuffer<float3> > positions;
	df::Resource< StructuredBuffer<float2> > uvs;
	df::Resource< StructuredBuffer<float2> > lightmap_uvs;
	df::Resource< Texture2D<float> >         sun_shadowmap;
};

struct DrawParameters
{
	df::Resource< Texture2D > albedo;
	float2 albedo_dim;

	float3 normal;
};

ConstantBuffer<PassParameters> pass : register(b1);
ConstantBuffer<DrawParameters> draw : register(b0);

struct VS_OUT
{
	float4 position           : SV_Position;
	float4 shadowmap_position : SHADOWMAP_POSITION;
	float2 uv                 : TEXCOORD;
	float2 lightmap_uv        : LIGHTMAP_TEXCOORD;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	float3 position    = pass.positions   .Get().Load(vertex_index);
	float2 uv          = pass.uvs         .Get().Load(vertex_index);
	float2 lightmap_uv = pass.lightmap_uvs.Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position           = mul(view.world_to_clip, float4(position, 1));
	OUT.shadowmap_position = mul(view.sun_matrix,    float4(position, 1));
	OUT.uv                 = uv;
	OUT.lightmap_uv        = lightmap_uv;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	Texture2D albedo = draw.albedo.Get();
	float3 color = albedo.Sample(df::s_linear_wrap, FatPixel(draw.albedo_dim, IN.uv)).rgb;

	//float3 light = draw.lightmap.Get().Sample(df::s_linear_wrap, IN.lightmap_uv).rgb;

	const float3 shadow_test_pos   = IN.shadowmap_position.xyz / IN.shadowmap_position.w;
	const float2 shadow_test_uv    = 0.5 + 0.5*float2(shadow_test_pos.x, -shadow_test_pos.y);
	const float  shadow_test_depth = shadow_test_pos.z;

	Texture2D<float> shadowmap = pass.sun_shadowmap.Get();
	const float shadowmap_depth = shadowmap.SampleLevel(df::s_linear_wrap, shadow_test_uv, 0);

	const float shadow_factor = shadow_test_depth > shadowmap_depth;

	float3 light = 0.0;
	light += shadow_factor * (view.sun_color * saturate(dot(draw.normal, view.sun_direction)));

	return float4(color*light, 1);
}
