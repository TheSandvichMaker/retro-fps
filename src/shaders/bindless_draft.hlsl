#include "bindless.hlsli"

struct ViewParameters
{
	float4x4 world_to_clip;
	float3   sun_direction;
};

struct PassParameters
{
	df::Resource< StructuredBuffer<float3> > positions;
	df::Resource< StructuredBuffer<float2> > uvs;
	df::Resource< StructuredBuffer<float2> > lightmap_uvs;
};

struct DrawParameters
{
	float3 normal;
	uint   vertex_offset;
};

ConstantBuffer<ViewParameters> view : register(b2);
ConstantBuffer<PassParameters> pass : register(b1);
ConstantBuffer<DrawParameters> draw : register(b0);

struct VS_OUT
{
	float4 position    : SV_Position;
	float2 uv          : TEXCOORD;
	float2 lightmap_uv : LIGHTMAP_TEXCOORD;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	uint vertex_offset = draw.vertex_offset;

	float3 position    = pass.positions   .Get().Load(vertex_offset + vertex_index);
	float2 uv          = pass.uvs         .Get().Load(vertex_offset + vertex_index);
	float2 lightmap_uv = pass.lightmap_uvs.Get().Load(vertex_offset + vertex_index);

	VS_OUT OUT;
	OUT.position    = mul(view.world_to_clip, float4(position, 1));
	OUT.uv          = uv;
	OUT.lightmap_uv = lightmap_uv;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
#if 0
	float3 albedo = draw.albedo  .Get().Sample(df::s_linear_wrap, IN.uv).rgb;
	float3 light  = draw.lightmap.Get().Sample(df::s_linear_wrap, IN.lightmap_uv).rgb;
#endif

	float3 light = 0.5 + 0.5*dot(draw.normal, view.sun_direction);

	return float4(light, 1);
}
