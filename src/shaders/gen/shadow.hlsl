// Generated by metagen.lua

#include "bindless.hlsli"

struct shadow_draw_parameters_t
{
	df::Resource< StructuredBuffer< float3 > > positions;
};

ConstantBuffer< shadow_draw_parameters_t > draw : register(b0);



#include "common.hlsli"

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
