#ifndef COMMON_HLSL
#define COMMON_HLSL

#define PI 3.1415926536

float max3(float3 v) { return max(v.x, max(v.y, v.z)); }

sampler sampler_point                    : register(s0);
sampler sampler_linear                   : register(s1);
sampler sampler_point_clamped            : register(s2);
sampler sampler_linear_clamped           : register(s3);
sampler sampler_fog                      : register(s4);
SamplerComparisonState sampler_shadowmap : register(s5);

struct VS_INPUT_POS
{
    float3 pos : POSITION;
};

struct VS_INPUT_IMMEDIATE
{
    float3 pos    : POSITION;
    float2 uv     : TEXCOORD;
    uint   col    : COLOR;
	float3 normal : NORMAL;
};

struct VS_INPUT_BRUSH
{
    float3 pos         : POSITION;
    float2 uv          : TEXCOORD;
    float2 uv_lightmap : TEXCOORD_LIGHTMAP;
    float3 normal      : NORMAL;
};

cbuffer cbuffer0 : register(b0)
{
    float4x4 view_matrix;
    float4x4 proj_matrix;
    float4x4 model_matrix;
    float4x4 sun_matrix;
    float3   light_direction;
    float    pad0;
    uint     frame_index;
    float    depth_bias;
    float    pad1;
    float    pad2;
    float3   fog_offset;
    float    pad3;
    float3   fog_dim;
    float    pad4;
    float3   sun_color;
    float    pad5;
    float3   sun_direction;
    float    fog_density;
    float    fog_absorption;
    float    fog_scattering;
    float    fog_phase_k;
}

float square(float x)
{
    return x*x;
}

float2 fat_pixel(float2 tex_dim, float2 in_uv)
{
    float2 texel_coord = in_uv*tex_dim;

    float2 uv = floor(texel_coord) + 0.5;
    
    float2 duv = fwidth(in_uv);

    float2 texel_derivatives = tex_dim*duv;
    float2 scale = 1 / texel_derivatives;

    uv += 1 - saturate((1 - frac(texel_coord))*scale);
    uv /= tex_dim;

    return uv;
}

void camera_ray(float2 uv, out float3 position, out float3 direction)
{
    float3 camera_x = {
        view_matrix[0][0],
        view_matrix[0][1],
        view_matrix[0][2],
    };

    float3 camera_y = {
        view_matrix[1][0],
        view_matrix[1][1],
        view_matrix[1][2],
    };

    float3 camera_z = {
        view_matrix[2][0],
        view_matrix[2][1],
        view_matrix[2][2],
    };

    position = -(view_matrix[0][3]*camera_x +
                 view_matrix[1][3]*camera_y +
                 view_matrix[2][3]*camera_z);

    float2 nds = 2*uv - 1;
    nds.y = -nds.y;

    float film_half_w = rcp(proj_matrix[0][0]);
    float film_half_h = rcp(proj_matrix[1][1]);

    direction = normalize(nds.x*camera_x*film_half_w +
                          nds.y*camera_y*film_half_h -
                          camera_z);
}

float4 pyramid_blur(Texture2D tex, sampler samp, float2 uv)
{
    float2 dim;
    tex.GetDimensions(dim.x, dim.y);

    float2 offset = 0.5 / dim;

    float4 result = 0;
    result += tex.Sample(samp, uv + float2( offset.x,  offset.y));
    result += tex.Sample(samp, uv + float2(-offset.x,  offset.y));
    result += tex.Sample(samp, uv + float2(-offset.x, -offset.y));
    result += tex.Sample(samp, uv + float2( offset.x, -offset.y));
    result *= 0.25f;

    return result;
}

float sample_pcf_3x3(Texture2D<float> shadowmap, float2 projected_pos, float current_depth, float bias)
{
    float biased_depth = current_depth + bias;

    float2 shadowmap_dim;
    shadowmap.GetDimensions(shadowmap_dim.x, shadowmap_dim.y);

    float4 sfrac;
    sfrac.xy = frac(projected_pos*shadowmap_dim - 0.5);
    sfrac.zw = 1.0 - sfrac.xy;

    float4 gather4_a = shadowmap.GatherRed(sampler_linear_clamped, projected_pos, int2(-1, -1));
    float4 gather4_b = shadowmap.GatherRed(sampler_linear_clamped, projected_pos, int2( 1, -1));
    float4 gather4_c = shadowmap.GatherRed(sampler_linear_clamped, projected_pos, int2(-1,  1));
    float4 gather4_d = shadowmap.GatherRed(sampler_linear_clamped, projected_pos, int2( 1,  1));

    float shadow = 0;
    shadow += dot(float4(sfrac.z, 1, sfrac.w, sfrac.z*sfrac.w), gather4_a > biased_depth);
    shadow += dot(float4(1, sfrac.x, sfrac.x*sfrac.w, sfrac.w), gather4_b > biased_depth);
    shadow += dot(float4(sfrac.z*sfrac.y, sfrac.y, 1, sfrac.z), gather4_c > biased_depth);
    shadow += dot(float4(sfrac.y, sfrac.x*sfrac.y, sfrac.x, 1), gather4_d > biased_depth);
    shadow /= 9.0;

    return shadow;
}

#endif
