#include "common.hlsli"

struct DrawParameters
{
	df::Resource< StructuredBuffer<float3> > positions;
};

ConstantBuffer<DrawParameters> draw : register(b0);

struct VS_OUT
{
	float4 position : SV_Position;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	float3 position = draw.positions.Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position = mul(view.sun_matrix, float4(position, 1));
	return OUT;
}
