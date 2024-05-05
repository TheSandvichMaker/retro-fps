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

sampler s_linear_wrap : register(s0, space100);

}

struct ShaderParameters
{
	df::Resource< StructuredBuffer<float3> > positions;
	df::Resource< StructuredBuffer<float4> > colors;
};

ConstantBuffer<ShaderParameters> parameters : register(b0);

struct VS_OUT
{
	float4 position : SV_Position;
	float4 color    : COLOR;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	float3 position = parameters.positions.Get().Load(vertex_index);
	float4 color    = parameters.colors   .Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position = float4(position, 1.0);
	OUT.color    = color;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	return IN.color;
}
