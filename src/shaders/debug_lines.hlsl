#include "common.hlsli"

struct LineVertex
{
	float3     pos;
	ColorRGBA8 color;
};

struct DrawParameters
{
	df::Resource< StructuredBuffer<LineVertex> > vertices;
};

ConstantBuffer<DrawParameters> draw : register(b0);

struct VS_OUT
{
	float4 position : SV_Position;
	float4 color    : COLOR;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	LineVertex vertex = draw.vertices.Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position = mul(view.world_to_clip, float4(vertex.pos, 1));
	OUT.color    = SRGBToLinear(Unpack(vertex.color));
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	return IN.color;
}
