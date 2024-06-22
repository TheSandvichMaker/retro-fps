// Generated by metagen.lua

struct view_parameters_t
{
	float4x4 sun_matrix;
	float4x4 view_to_clip;
	float4x4 world_to_clip;
	float4x4 world_to_view;
	float3 sun_color;
	float fog_absorption;
	float3 sun_direction;
	float fog_density;
	float2 view_size;
	float fog_phase_k;
	float fog_scattering;
	uint frame_index;
};

ConstantBuffer< view_parameters_t > view : register(b2);
