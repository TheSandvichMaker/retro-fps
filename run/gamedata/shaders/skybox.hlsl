#include "common.hlsl"

struct VS_INPUT
{
    float3 pos : POSITION;
};

struct PS_INPUT
{
    float4 pos      : SV_POSITION;
    float3 texcoord : TEXCOORD;
};

sampler     cubemap_sampler : register(s0);
TextureCube cubemap         : register(t0);

PS_INPUT vs(VS_INPUT IN)
{
    float4x4 swizzle = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };

    PS_INPUT OUT;
    OUT.pos      = mul(mul(camera_projection, swizzle), float4(IN.pos, 1));
    OUT.texcoord = IN.pos;
    return OUT;
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    return cubemap.Sample(cubemap_sampler, IN.texcoord);
}
