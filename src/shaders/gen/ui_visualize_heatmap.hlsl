// Generated by metagen.lua

#include "bindless.hlsli"

#include "common.hlsli"

struct ui_visualize_heatmap_draw_parameters_t
{
	df::Resource< Texture2D< float4 > > heatmap;
	float scale;
};

ConstantBuffer< ui_visualize_heatmap_draw_parameters_t > draw : register(b0);



float4 MainPS(FullscreenTriangleOutVS IN) : SV_Target
{
	uint2 co = uint2(IN.pos.xy);

	Texture2D heatmap = draw.heatmap.Get();

	float  heatmap_value = draw.scale * heatmap.SampleLevel(df::s_linear_clamped, IN.uv, 0).r;
	float3 result        = rgb_from_hsv(float3(heatmap_value, 1.0, 1.0));

	return float4(result, 0.5);
}
