#pragma once

typedef struct brush_pass_parameters_t
{
	rhi_buffer_srv_t lm_uvs;
	v2_t shadowmap_dim;
	uint32_t pad0;
	rhi_buffer_srv_t normals;
	v3u_t pad1;
	rhi_buffer_srv_t positions;
	v3u_t pad2;
	rhi_texture_srv_t sun_shadowmap;
	v3u_t pad3;
	rhi_buffer_srv_t uvs;
	v3u_t pad4;
} brush_pass_parameters_t;

typedef struct brush_draw_parameters_t
{
	rhi_texture_srv_t albedo;
	v2_t albedo_dim;
	rhi_texture_srv_t lightmap;
	v2_t lightmap_dim;
} brush_draw_parameters_t;

