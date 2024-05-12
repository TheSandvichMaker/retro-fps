#include "bindless.hlsli"

struct PassParameters
{
	df::Resource< StructuredBuffer<float3> > positions;
	df::Resource< StructuredBuffer<float4> > colors;
	df::Resource< StructuredBuffer<float2> > uvs;
};

struct ShaderParameters
{
	float4 offset;
	float4 color;
	df::Resource< Texture2D > albedo;
};

ConstantBuffer<PassParameters>   pass       : register(b1);
ConstantBuffer<ShaderParameters> parameters : register(b0);

struct VS_OUT
{
	float4 position : SV_Position;
	float4 color    : COLOR;
	float2 uv       : TEXCOORD;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	float3 position = pass.positions.Get().Load(vertex_index);
	float4 color    = pass.colors   .Get().Load(vertex_index);
	float2 uv       = pass.uvs      .Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position = float4(position, 1.0) + parameters.offset;
	OUT.color    = parameters.color + color;
	OUT.uv       = uv;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	float4 albedo = parameters.albedo.Get().SampleLevel(df::s_linear_wrap, IN.uv, 0.0);
	return albedo;// * IN.color;
}
