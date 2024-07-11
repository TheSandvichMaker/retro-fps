#ifndef DF_BINDLESS_HLSLI
#define DF_BINDLESS_HLSLI

namespace df
{

template <typename T>
struct Resource
{
	uint index;

	T Get()
	{
		return ResourceDescriptorHeap[index];
	}

	T GetNonUniform()
	{
		return ResourceDescriptorHeap[NonUniformResourceIndex(index)];
	}
};

struct Sampler
{
	uint index;

	sampler Get()
	{
		return SamplerDescriptorHeap[index];
	}

	sampler GetNonUniform()
	{
		return SamplerDescriptorHeap[NonUniformResourceIndex(index)];
	}
};

sampler s_aniso_wrap     : register(s0, space100);
sampler s_aniso_clamped  : register(s1, space100);
sampler s_linear_wrap    : register(s2, space100);
sampler s_linear_clamped : register(s3, space100);
sampler s_linear_border  : register(s4, space100);
sampler s_point_wrap     : register(s5, space100);
sampler s_point_clamped  : register(s6, space100);

}

#endif