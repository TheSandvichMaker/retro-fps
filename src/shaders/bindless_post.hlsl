#include "common.hlsli"

struct DrawParameters
{
	df::Resource< Texture2D > hdr_color;
};

ConstantBuffer<DrawParameters> draw : register(b0);

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

VS_OUT MainVS(uint id : SV_VertexID)
{
    VS_OUT OUT;
    OUT.uv  = uint2(id, id << 1) & 2;
    OUT.pos = float4(lerp(float2(-1, 1), float2(1, -1), OUT.uv), 0, 1);
    return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	uint2  co    = uint2(IN.pos.xy);
	float4 color = draw.hdr_color.Get().Load(uint3(co, 0));

	// tonemap
	color.rgb = 1.0 - exp(-color.rgb);

	return color;
}
