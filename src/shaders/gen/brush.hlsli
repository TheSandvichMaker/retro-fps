// Generated by metagen.lua from brush.metashader

#include "bindless.hlsli"

struct brush_draw_parameters_t
{
	df::Resource< Texture2D< float3 > > albedo;
	float3 normal;
	df::Resource< Texture2D< float3 > > lightmap;
	float2 albedo_dim;
	uint pad0;
	float2 lightmap_dim;
};

struct brush_pass_parameters_t
{
	df::Resource< StructuredBuffer< float2 > > lm_uvs;
	uint3 pad0;
	df::Resource< StructuredBuffer< float3 > > positions;
	uint3 pad1;
	df::Resource< Texture2D< float > > sun_shadowmap;
	uint3 pad2;
	df::Resource< StructuredBuffer< float2 > > uvs;
};

