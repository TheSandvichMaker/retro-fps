#pragma once

#include "bindless.hlsli"

struct ViewParameters
{
	float4x4 world_to_clip;
	float4x4 sun_matrix;
	float3   sun_direction;
	float3   sun_color;
};

ConstantBuffer<ViewParameters> view : register(b2);

float2 FatPixel(float2 texture_dim, float2 in_uv)
{
	float2 texel_coord = in_uv * texture_dim;

	float2 uv = floor(texel_coord) + 0.5;
	uv += 1.0 - saturate((1.0 - frac(texel_coord)) / fwidth(in_uv));
	uv *= rcp(texture_dim);

	return uv;
}
