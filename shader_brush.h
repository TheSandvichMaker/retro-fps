typedef struct brush_draw_parameters_t
{
	alignas(16) v2_t albedo_dim;
	alignas(16) v2_t lightmap_dim;
	alignas(16) rhi_texture_srv_t albedo;
	alignas(16) rhi_texture_srv_t lightmap;
} brush_draw_parameters_t;

typedef struct brush_pass_parameters_t
{
	alignas(16) v2_t shadowmap_dim;
	alignas(16) rhi_buffer_srv_t lm_uvs;
	alignas(16) rhi_buffer_srv_t normals;
	alignas(16) rhi_buffer_srv_t positions;
	alignas(16) rhi_texture_srv_t sun_shadowmap;
	alignas(16) rhi_buffer_srv_t uvs;
} brush_pass_parameters_t;

