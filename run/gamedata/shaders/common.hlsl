#ifndef COMMON_HLSL
#define COMMON_HLSL

float max3(float3 v) { return max(v.x, max(v.y, v.z)); }

sampler sampler_point  : register(s0);
sampler sampler_linear : register(s1);

sampler sampler_point_clamped  : register(s2);
sampler sampler_linear_clamped : register(s3);

struct VS_INPUT_POS
{
    float3 pos : POSITION;
};

struct VS_INPUT_IMMEDIATE
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
    uint   col : COLOR;
};

struct VS_INPUT_BRUSH
{
    float3 pos         : POSITION;
    float2 uv          : TEXCOORD;
    float2 uv_lightmap : TEXCOORD_LIGHTMAP;
};

cbuffer cbuffer0 : register(b0)
{
    float4x4 view_matrix;
    float4x4 proj_matrix;
    float4x4 model_matrix;
    uint     frame_index;
    float    depth_bias;
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

    float2 nds = 2*uv - 1;

    float film_half_w = rcp(proj_matrix[0][0]);
    float film_half_h = rcp(proj_matrix[1][1]);

    direction = normalize(nds.x*camera_x*film_half_w +
                          nds.y*camera_y*film_half_h -
                          camera_z);

    position = -(view_matrix[0][3]*camera_x +
                 view_matrix[1][3]*camera_y +
                 view_matrix[2][3]*camera_z);
}

#endif
