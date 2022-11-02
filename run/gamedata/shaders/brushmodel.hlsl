#include "common.hlsl"

struct PS_INPUT
{
    float4 pos         : SV_POSITION;
    float2 uv          : TEXCOORD;
    float2 uv_lightmap : TEXCOORD_LIGHTMAP;
    float4 col         : COLOR;
};

Texture2D albedo   : register(t0);
Texture2D lightmap : register(t1);

PS_INPUT vs(VS_INPUT_BRUSH IN)
{
    PS_INPUT OUT;
    OUT.pos         = mul(mul(camera_projection, model_transform), float4(IN.pos, 1));
    OUT.uv          = IN.uv;
    OUT.uv_lightmap = IN.uv_lightmap;
    OUT.col         = float4(IN.col, 1);
    return OUT;
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    float2 dim;
    albedo.GetDimensions(dim.x, dim.y);

    float2 uv = fat_pixel(dim, IN.uv);

    float4 tex      = albedo.Sample(sampler_linear, uv);
    float4 lighting = lightmap.Sample(sampler_linear_clamped, IN.uv_lightmap);
    float4 col      = IN.col*float4(lighting.xyz*tex.xyz, 1.0f);
    return col;
}
