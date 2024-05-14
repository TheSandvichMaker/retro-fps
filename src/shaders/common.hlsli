#pragma once

#include "bindless.hlsli"

struct ViewParameters
{
	float4x4 world_to_clip;
	float3   sun_direction;
};

ConstantBuffer<ViewParameters> view : register(b2);
