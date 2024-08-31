#pragma once

//
// draw packet API
//

typedef struct rhi_indirect_draw_t
{
	uint32_t vertex_count;
	uint32_t instance_count;
	 int32_t vertex_offset;
	uint32_t instance_offset;
} rhi_indirect_draw_t;

typedef struct rhi_indirect_draw_indexed_t
{
	uint32_t index_count;
	uint32_t instance_count;
	uint32_t index_offset;
	 int32_t vertex_offset;
	uint32_t instance_offset;
} rhi_indirect_draw_indexed_t;

typedef struct rhi_draw_packet_t
{
								 // 32 bit handles - 64 bit handles
	rhi_pso_t    pso;            // 4                8
	rhi_buffer_t args_buffer;    // 8                16
	uint32_t     args_offset;    // 12               20
	rhi_buffer_t index_buffer;   // 16               24
	uint32_t     push_constants; // 20               28
	uint32_t     params[3];      // 32               48
} rhi_draw_packet_t;

typedef struct rhi_draw_stream_t
{
	uint32_t           count;
	rhi_draw_packet_t *packets;
} rhi_draw_stream_t;

typedef struct rhi_draw_stream_params_t
{
	rhi_graphics_pass_color_attachment_t         render_targets[RhiMaxRenderTargetCount];
	rhi_graphics_pass_depth_stencil_attachment_t depth_stencil;

	rhi_primitive_topology_t topology;
	rhi_viewport_t           viewport;
	rect2i_t                 scissor_rect;

	void *push_constants_buffer;
} rhi_draw_stream_params_t;

fn void rhi_draw_stream(const rhi_draw_stream_t *stream, const rhi_draw_stream_params_t *params);