#include "bindless.hlsli"

struct ViewParameters
{
	float4x4 world_to_clip;
};

struct PassParameters
{
	df::Resource< StructuredBuffer<float3> > position_buffer;
	df::Resource< StructuredBuffer<float2> > texcoord_buffer;
	df::Resource< StructuredBuffer<float2> > ligtmap_texcoord_buffer;
};

struct Vertex
{
	float4 position;
	float4 color;
	float2 uv;
};

struct ShaderParameters
{
	ConstantBuffer<ViewParameters> view : register(b0);
	ConstantBuffer<PassParameters> pass : register(b1);

	df::Resource< Texture2D > albedo;
	df::Resource< Texture2D > lightmap;
};

ConstantBuffer<ShaderParameters> parameters : register(b2);

struct VS_OUT
{
	float4 position          : SV_Position;
	float4 texcoord          : TEXCOORD;
	float2 lightmap_texcoord : LIGHTMAP_TEXCOORD;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	float3 position          = pass.position_buffer         .Get().Load(vertex_index);
	float2 texcoord          = pass.texcoord_buffer         .Get().Load(vertex_index);
	float2 lightmap_texcoord = pass.lightmap_texcoord_buffer.Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position          = mul(view.world_to_clip, float4(position, 1));
	OUT.texcoord          = texcoord;
	OUT.lightmap_texcoord = lightmap_texcoord;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	float3 albedo = parameters.albedo  .Get().Sample(df::s_linear_wrap, IN.texcoord).rgb;
	float3 light  = parameters.lightmap.Get().Sample(df::s_linear_wrap, IN.lightmap_texcoord).rgb;

	return float4(albedo * light, 1);
}
