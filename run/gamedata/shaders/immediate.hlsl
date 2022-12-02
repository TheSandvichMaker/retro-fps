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
    PS_INPUT OUT;
    OUT.pos = mul(mul(proj_matrix, view_matrix), float4(IN.pos, 1));
    OUT.pos.z += depth_bias;
    OUT.uv  = IN.uv;

    OUT.col = float4((float)((IN.col >>  0) & 0xFF) / 255.0f,
                     (float)((IN.col >>  8) & 0xFF) / 255.0f,
                     (float)((IN.col >> 16) & 0xFF) / 255.0f,
                     (float)((IN.col >> 24) & 0xFF) / 255.0f);
    OUT.col.xyz = OUT.col.xyz*OUT.col.xyz;

    return OUT;
}

float r_dither(float2 co)
{
	const float2 magic = float2(0.75487766624669276, 0.569840290998);
    return frac(dot(co, magic));
}

float remap_tri(float n)
{
    float orig = n * 2.0 - 1.0;
    n = orig * rsqrt(abs(orig));
    return max(-1.0, n) - sign(orig);
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    float4 tex = texture0.Sample(sampler_point, IN.uv);

    float4 result = IN.col*tex;
    result.rgb += remap_tri(r_dither(IN.pos.xy)) / 255.0f;

    return result;
}
