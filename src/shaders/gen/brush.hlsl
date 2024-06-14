#include "bindless.hlsli"

struct brush_pass_parameters_t
{
	df::Resource< StructuredBuffer< float2 > > lm_uvs;
	float2 shadowmap_dim;
	uint pad0;
	df::Resource< StructuredBuffer< float3 > > normals;
	uint3 pad1;
	df::Resource< StructuredBuffer< float3 > > positions;
	uint3 pad2;
	df::Resource< Texture2D< float > > sun_shadowmap;
	uint3 pad3;
	df::Resource< StructuredBuffer< float2 > > uvs;
	uint3 pad4;
};

ConstantBuffer< brush_pass_parameters_t > pass : register(b1);

struct brush_draw_parameters_t
{
	float3 normal;
	df::Resource< Texture2D< float3 > > albedo;
	float2 albedo_dim;
	float2 lightmap_dim;
	df::Resource< Texture2D< float3 > > lightmap;
};

ConstantBuffer< brush_draw_parameters_t > draw : register(b0);


	#include "common.hlsli"

	struct VS_OUT
	{
		float4 position           : SV_Position;
		float4 shadowmap_position : SHADOWMAP_POSITION;
		float2 uv                 : TEXCOORD;
		float2 lightmap_uv        : LIGHTMAP_TEXCOORD;
	};

	VS_OUT MainVS(uint vertex_index : SV_VertexID)
	{
		float3 position    = pass.positions.Get().Load(vertex_index);
		float2 uv          = pass.uvs      .Get().Load(vertex_index);
		float2 lightmap_uv = pass.lm_uvs   .Get().Load(vertex_index);

		VS_OUT OUT;
		OUT.position           = mul(view.world_to_clip, float4(position, 1));
		OUT.shadowmap_position = mul(view.sun_matrix,    float4(position, 1));
		OUT.uv                 = uv;
		OUT.lightmap_uv        = lightmap_uv;
		return OUT;
	}

	float4 MainPS(VS_OUT IN) : SV_Target
	{
		Texture2D<float3> albedo = draw.albedo.Get();

		float2 albedo_dim;
		albedo.GetDimensions(albedo_dim.x, albedo_dim.y);

		float3 color = albedo.Sample(df::s_aniso_wrap, FatPixel(albedo_dim, IN.uv)).rgb;

		Texture2D<float3> lightmap = draw.lightmap.Get();

		float2 lightmap_dim;
		lightmap.GetDimensions(lightmap_dim.x, lightmap_dim.y);

		float3 light = lightmap.Sample(df::s_aniso_wrap, FatPixel(lightmap_dim, IN.lightmap_uv)).rgb;

		const float3 shadow_test_pos   = IN.shadowmap_position.xyz / IN.shadowmap_position.w;
		const float2 shadow_test_uv    = 0.5 + float2(0.5, -0.5)*shadow_test_pos.xy;
		const float  shadow_test_depth = shadow_test_pos.z;

		float shadow_factor = 1.0;

		if (shadow_test_depth >= 0.0 && shadow_test_depth <= 1.0)
		{
			const float bias = max(0.025*(1.0 - max(0, dot(draw.normal, view.sun_direction))), 0.010);
			const float biased_depth = shadow_test_depth + bias;

			float2 shadowmap_dim;
			pass.sun_shadowmap.Get().GetDimensions(shadowmap_dim.x, shadowmap_dim.y);

			shadow_factor = SampleShadowPCF3x3(pass.sun_shadowmap.Get(), shadowmap_dim, shadow_test_uv, biased_depth);
		}

		light += shadow_factor*(view.sun_color*saturate(dot(draw.normal, view.sun_direction)));

		return float4(color*light, 1);
	}

	