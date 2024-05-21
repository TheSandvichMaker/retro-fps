#pragma once

#include "bindless.hlsli"

struct ViewParameters
{
	float4x4 world_to_clip;
	float4x4 sun_matrix;
	float3   sun_direction;
	float3   sun_color;
	float2   view_size;
};

ConstantBuffer<ViewParameters> view : register(b2);

float QuasirandomDither(float2 co)
{
	const float2 magic = float2(0.75487766624669276, 0.569840290998);
    return frac(dot(co, magic));
}

float RemapTriPDF(float n)
{
    float orig = n * 2.0 - 1.0;
    n = orig * rsqrt(abs(orig));
    return max(-1.0, n) - sign(orig);
}

struct ColorRGBA8
{
	uint color;
};

float4 Unpack(ColorRGBA8 rgba8)
{
	uint color = rgba8.color;

	float4 result = float4((float)((color >>  0) & 0xFF) / 255.0f,
						   (float)((color >>  8) & 0xFF) / 255.0f,
						   (float)((color >> 16) & 0xFF) / 255.0f,
						   (float)((color >> 24) & 0xFF) / 255.0f);
	return result;
}

float3 LinearToSRGB(float3 lin)
{
	// TODO: do better
	return sqrt(lin);
}

float3 SRGBToLinear(float3 srgb)
{
	// TODO: do better
	return srgb * srgb;
}

float4 LinearToSRGB(float4 lin)
{
	return float4(LinearToSRGB(lin.rgb), lin.a);
}

float4 SRGBToLinear(float4 srgb)
{
	return float4(SRGBToLinear(srgb.rgb), srgb.a);
}

float2 FatPixel(float2 texture_dim, float2 in_uv)
{
	float2 texel_coord = in_uv * texture_dim;

	float2 uv = floor(texel_coord) + 0.5;
	uv += 1.0 - saturate((1.0 - frac(texel_coord)) / fwidth(in_uv));
	uv *= rcp(texture_dim);

	return uv;
}

float SampleShadowPCF3x3(Texture2D<float> shadowmap, float2 shadowmap_dim, float2 projected_pos, float test_depth)
{
    float4 sfrac;
    sfrac.xy = frac(projected_pos*shadowmap_dim - 0.5);
    sfrac.zw = 1.0 - sfrac.xy;

    float4 gather4_a = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2(-1, -1));
    float4 gather4_b = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2( 1, -1));
    float4 gather4_c = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2(-1,  1));
    float4 gather4_d = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2( 1,  1));

    float shadow = 0;
    shadow += dot(float4(sfrac.z, 1, sfrac.w, sfrac.z*sfrac.w), gather4_a < test_depth);
    shadow += dot(float4(1, sfrac.x, sfrac.x*sfrac.w, sfrac.w), gather4_b < test_depth);
    shadow += dot(float4(sfrac.z*sfrac.y, sfrac.y, 1, sfrac.z), gather4_c < test_depth);
    shadow += dot(float4(sfrac.y, sfrac.x*sfrac.y, sfrac.x, 1), gather4_d < test_depth);
    shadow /= 9.0;

    return shadow;
}
