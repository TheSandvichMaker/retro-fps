#include "common.hlsli"

struct PassParameters
{
	df::Resource< StructuredBuffer<float3> > positions;
	df::Resource< StructuredBuffer<float2> > uvs;
	df::Resource< StructuredBuffer<float2> > lightmap_uvs;
	df::Resource< Texture2D<float> >         sun_shadowmap;
	float2                                   shadowmap_dim;
};

struct DrawParameters
{
	df::Resource< Texture2D > albedo;
	float2                    albedo_dim;

	df::Resource< Texture2D > lightmap;
	float2                    lightmap_dim;

	float3 normal;
};

ConstantBuffer<PassParameters> pass : register(b1);
ConstantBuffer<DrawParameters> draw : register(b0);

struct VS_OUT
{
	float4 position           : SV_Position;
	float4 shadowmap_position : SHADOWMAP_POSITION;
	float2 uv                 : TEXCOORD;
	float2 lightmap_uv        : LIGHTMAP_TEXCOORD;
};

VS_OUT MainVS(uint vertex_index : SV_VertexID)
{
	float3 position    = pass.positions   .Get().Load(vertex_index);
	float2 uv          = pass.uvs         .Get().Load(vertex_index);
	float2 lightmap_uv = pass.lightmap_uvs.Get().Load(vertex_index);

	VS_OUT OUT;
	OUT.position           = mul(view.world_to_clip, float4(position, 1));
	OUT.shadowmap_position = mul(view.sun_matrix,    float4(position, 1));
	OUT.uv                 = uv;
	OUT.lightmap_uv        = lightmap_uv;
	return OUT;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	Texture2D albedo = draw.albedo.Get();
	float3 color = albedo.Sample(df::s_linear_wrap, FatPixel(draw.albedo_dim, IN.uv)).rgb;

	Texture2D lightmap = draw.lightmap.Get();
	float3 light = lightmap.Sample(df::s_linear_wrap, FatPixel(draw.lightmap_dim, IN.lightmap_uv)).rgb;

	const float3 shadow_test_pos   = IN.shadowmap_position.xyz / IN.shadowmap_position.w;
	const float2 shadow_test_uv    = 0.5 + float2(0.5, -0.5)*shadow_test_pos.xy;
	const float  shadow_test_depth = shadow_test_pos.z;

	float shadow_factor = 1.0;

	if (shadow_test_depth >= 0.0 && shadow_test_depth <= 1.0)
	{
        const float bias = max(0.025*(1.0 - max(0, dot(draw.normal, view.sun_direction))), 0.010);
		const float biased_depth = shadow_test_depth + bias;

		shadow_factor = SampleShadowPCF3x3(pass.sun_shadowmap.Get(), pass.shadowmap_dim, shadow_test_uv, biased_depth);
	}

	light += shadow_factor*(view.sun_color*saturate(dot(draw.normal, view.sun_direction)));

	return float4(color*light, 1);
}
