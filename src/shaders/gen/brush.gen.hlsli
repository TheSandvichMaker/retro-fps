#pragma once

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

struct brush_draw_parameters_t
{
	df::Resource< Texture2D< float3 > > albedo;
	float2 albedo_dim;
	df::Resource< Texture2D< float3 > > lightmap;
	float2 lightmap_dim;
};

