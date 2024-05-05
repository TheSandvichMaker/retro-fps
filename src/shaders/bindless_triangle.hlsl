#include "bindless.hlsli"

struct PassParameters
{
	df::Resource< StructuredBuffer<float3> > positions;
	df::Resource< StructuredBuffer<float4> > colors;
};

struct ShaderParameters
{
	float4 offset;
	float4 color;
};

ConstantBuffer<PassParameters>   pass       : register(b1);
ConstantBuffer<ShaderParameters> parameters : register(b0);

struct VS_OUT
{
	float4 position : SV_Position;
	float4 color    : COLOR;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	float3 position = pass.positions.Get().Load(vertex_index);
	float4 color    = pass.colors   .Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position = float4(position, 1.0) + parameters.offset;
	OUT.color    = parameters.color;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	return IN.color;
}
