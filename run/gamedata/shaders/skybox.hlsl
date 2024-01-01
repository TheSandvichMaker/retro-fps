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
    PS_INPUT OUT;
    OUT.pos      = mul(skybox_matrix, float4(IN.pos, 1));
    OUT.texcoord = IN.pos;
    return OUT;
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    return cubemap.Sample(cubemap_sampler, IN.texcoord);
}
