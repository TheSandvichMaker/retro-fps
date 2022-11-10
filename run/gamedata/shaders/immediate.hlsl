#include "common.hlsl"

struct VS_INPUT
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
    uint   col : COLOR;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
    float4 col : COLOR;
};

sampler   sampler0 : register(s0);
Texture2D texture0 : register(t0);

PS_INPUT vs(VS_INPUT IN)
{
    PS_INPUT OUT;
    OUT.pos = mul(mul(camera_projection, model_transform), float4(IN.pos, 1));
    OUT.pos.z += depth_bias;
    OUT.uv  = IN.uv;

    OUT.col = float4((float)((IN.col >>  0) & 0xFF) / 255.0f,
                     (float)((IN.col >>  8) & 0xFF) / 255.0f,
                     (float)((IN.col >> 16) & 0xFF) / 255.0f,
                     (float)((IN.col >> 24) & 0xFF) / 255.0f);

    return OUT;
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    float4 tex = texture0.Sample(sampler0, IN.uv);
    return IN.col*tex;
}
