#pragma once

struct ViewParameters
{
	float4x4 world_to_clip;
};

ConstantBuffer<ViewParameters> view : register(b2);
