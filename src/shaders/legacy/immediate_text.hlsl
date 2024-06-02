#include "common.hlsl"

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
    float4 col : COLOR;
};

Texture2D texture0 : register(t0);

PS_INPUT vs(VS_INPUT_IMMEDIATE IN)
{
	float4x4 mvp = mul(proj_matrix, view_matrix);

    PS_INPUT OUT;
    OUT.pos = mul(mvp, float4(IN.pos, 1));
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
    float tex = texture0.Sample(sampler_linear, IN.uv).r;
	tex = sqrt(tex);

    float4 result = IN.col*tex;
    return result;
}
