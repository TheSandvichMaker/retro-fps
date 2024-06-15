// Generated by metagen.lua

#include "bindless.hlsli"

struct post_draw_parameters_t
{
	df::Resource< Texture2DMS< float4 > > hdr_color;
	uint sample_count;
};

ConstantBuffer< post_draw_parameters_t > draw : register(b0);


	#include "common.hlsli"

	struct VS_OUT
	{
		float4 pos : SV_POSITION;
		float2 uv  : TEXCOORD;
	};

	VS_OUT MainVS(uint id : SV_VertexID)
	{
		VS_OUT OUT;
		OUT.uv  = uint2(id, id << 1) & 2;
		OUT.pos = float4(lerp(float2(-1, 1), float2(1, -1), OUT.uv), 0, 1);
		return OUT;
	}

	float4 MainPS(VS_OUT IN) : SV_Target
	{
		uint2 co = uint2(IN.pos.xy);

		Texture2DMS<float4> tex_color = draw.hdr_color.Get();

		float4 sum = 0.0;
		for (uint i = 0; i < draw.sample_count; i++)
		{
			float4 color = tex_color.Load(co, i);

			float2 sample_position = tex_color.GetSamplePosition(i);

			float weight = 1.0 ; //smoothstep(8, 0, sample_position.x)*smoothstep(8, 0, sample_position.y);

			// tonemap
			color.rgb = 1.0 - exp(-color.rgb);
			color.rgb *= weight;

			sum.rgb += color.rgb;
			sum.a   += weight;
		}

		sum.rgb *= rcp(sum.a);

		return float4(sum.rgb, 1.0);
	}

	