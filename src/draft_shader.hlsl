#include "bindless.hlsli"

struct ViewConstants
{
	float4x4 world_to_clip;
};

struct PassConstants
{
	df::Resource<Texture2D> albedo;
	df::Sampler             albedo_sampler;

	df::Resource<StructuredBuffer<float3>> position_buffer;
	df::Resource<StructuredBuffer<float4>> color_buffer;
	df::Resource<StructuredBuffer<float2>> uv_buffer;
};

struct Vertex
{
	float4 position;
	float4 color;
	float2 uv;
};

struct ShaderParameters
{
	df::Resource<ConstantBuffer<ViewConstants>> view;
	df::Resource<ConstantBuffer<PassConstants>> pass;

	float4x4 transform;
};

ConstantBuffer<ShaderParameters> parameters : register(b0);

struct VS_OUT
{
	float4 position : SV_Position;
	float4 color    : COLOR;
	float2 uv       : TEXCOORD;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	ConstantBuffer<ViewConstants> view = parameters.view.Get();
	ConstantBuffer<PassConstants> pass = parameters.pass.Get();

	float3 position = pass.position_buffer.Get().Load(vertex_index);
	float4 color    = pass.color_buffer   .Get().Load(vertex_index);
	float2 uv       = pass.uv_buffer      .Get().Load(vertex_index);

	float4x4 mvp = mul(view.world_to_clip, parameters.transform);

	VS_OUT OUT;
	OUT.position = mul(mvp, position);
	OUT.color    = color;
	OUT.uv       = uv;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	ConstantBuffer<PassConstants> pass = parameters.pass.Get();

	Texture2D abledo_texture = pass.albedo.Get();
	sampler   albedo_sampler = pass.abledo_sampler.Get();

	float3 albedo = abledo_texture.Sample(albedo_sampler, IN.uv);
	float3 color  = albedo * IN.color;

	return color;
}

