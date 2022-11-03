#include "common.hlsl"

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

Texture2DMS<float3> rendertarget;

PS_INPUT vs(uint id : SV_VertexID)
{
    PS_INPUT OUT;
    OUT.uv  = uint2(id, id << 1) & 2;
    OUT.pos = float4(lerp(float2(-1, 1), float2(1, -1), OUT.uv), 0, 1);
    return OUT;
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    uint2 dim; uint sample_count;
    rendertarget.GetDimensions(dim.x, dim.y, sample_count);

    uint2 co = IN.uv*dim;

    float3 color = 0;
    for (uint i = 0; i < sample_count; i++)
    {
        color += rendertarget.Load(co, i);
    }
    color *= rcp(sample_count);
    color = pow(color, 1.0 / 2.23);

    return float4(color, 1);
}
