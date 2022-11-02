#ifndef COMMON_HLSL
#define COMMON_HLSL

sampler sampler_point  : register(s0);
sampler sampler_linear : register(s1);

sampler sampler_point_clamped  : register(s2);
sampler sampler_linear_clamped : register(s3);

struct VS_INPUT_POS
{
    float3 pos : POSITION;
};

struct VS_INPUT_POS_TEX_COL
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
    float3 col : COLOR;
};

struct VS_INPUT_BRUSH
{
    float3 pos         : POSITION;
    float2 uv          : TEXCOORD;
    float2 uv_lightmap : TEXCOORD_LIGHTMAP;
    float3 col         : COLOR;
};

cbuffer cbuffer0 : register(b0)
{
    float4x4 camera_projection;
    float4x4 light_projection;
    float4x4 model_transform;
}

float2 fat_pixel(float2 tex_dim, float2 in_uv)
{
    float2 texel_coord = in_uv*tex_dim;

    float2 uv = floor(texel_coord) + 0.5;
    
    float2 duv;
    duv.x = length(float2(ddx(in_uv.x), ddy(in_uv.x)));
    duv.y = length(float2(ddx(in_uv.y), ddy(in_uv.y)));

    float2 texel_derivatives = tex_dim*duv;
    float2 scale = 1 / texel_derivatives;

    uv += 1 - saturate((1 - frac(texel_coord))*scale);
    uv /= tex_dim;

    return uv;
}

#endif
