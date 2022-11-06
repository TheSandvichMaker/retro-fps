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

float4 hash43n(float3 p)
{
    p  = frac(p * float3(5.3987, 5.4421, 6.9371));
    p += dot(p.yzx, p.xyz  + float3(21.5351, 14.3137, 15.3247));
    return frac(float4(p.x * p.y * 95.4307, p.x * p.y * 97.5901, p.x * p.z * 93.8369, p.y * p.z * 91.6931 ));
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
    color = pow(abs(color), 1.0 / 2.23);

    float3 dither = hash43n(float3(IN.uv, frame_index)).xyz;
    color += (dither - 0.5) / 255.0;

    return float4(saturate(color), 1);
}
