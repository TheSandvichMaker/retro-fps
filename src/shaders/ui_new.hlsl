#include "common.hlsli"

// TODO: Provide as define from CPU side automatically
#define UiCommandStride (64)

void decode_ui_header(uint header, out uint kind, out uint group, out uint clip_rect)
{
	kind      = (header >>  0) & 0xFF;
	group     = (header >>  8) & 0xFF;
	clip_rect = (header >> 16);
}

#define UiCommand_box    1
#define UiCommand_image  2
#define UiCommand_circle 3

struct ui_command_box_t
{
	float2 origin;
	float2 radius;
	float4 roundedness;
	uint   colors[4];
	float  shadow_radius;
	float  shadow_amount;
	float  inner_radius;
};

ui_command_box_t decode_ui_box(uint4 load0, uint4 load1, uint4 load2, uint4 load3)
{
	ui_command_box_t box;
	box.origin        = asfloat(load0.yz);
	box.radius        = asfloat(uint2(load0.w, load1.x));
	box.roundedness   = asfloat(uint4(load1.yzw, load2.x));
	box.colors[0]     = load2.y;
	box.colors[1]     = load2.z;
	box.colors[2]     = load2.w;
	box.colors[3]     = load3.x;
	box.shadow_radius = asfloat(load3.y);
	box.shadow_amount = asfloat(load3.z);
	box.inner_radius  = asfloat(load3.w);
	return box;
}

struct draw_params_t
{
	uint                                             command_count;
	df::Resource< ByteAddressBuffer >                command_buffer;
	df::Resource< StructuredBuffer<ui_clip_rect_t> > clip_rect_buffer;
};

ConstantBuffer<draw_params_t> draw : register(b0);

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

float sdf_box_rounded(float2 p, float2 b, float4 r)
{
	r.xy = (p.x > 0.0f) ? r.xy : r.zw;
	r.x  = (p.y > 0.0f) ? r.x  : r.y;
	float2 q = abs(p) - b + r.x;
	return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;
}

float4 MainPS(VS_OUT IN) : SV_Target
{
	uint                             command_count    = draw.command_count;
	ByteAddressBuffer                command_buffer   = draw.command_buffer.Get();
	StructuredBuffer<ui_clip_rect_t> clip_rect_buffer = draw.clip_rect_buffer.Get();

	float4 color = 0.0;

	uint  current_group = 0;
	float sdf           = 10000000.0;

	for (uint command_index = 0; command_index < command_count; command_index++)
	{
		uint byte_offset = command_index*UiCommandStride;

		uint read_offset = byte_offset;
		uint4 load0 = buffer.Load4(read_offset); read_offset += 4*4;

		uint kind, group, clip_rect_index;
		decode_ui_header(load0.x, kind, group, clip_rect_index);

		if (current_group != group)
		{
			current_group = group;

			// do shadow based on sdf

			// reset sdf
			sdf = 10000000.0;
		}

		switch (kind)
		{
			case UiCommand_box:
			{
				uint4 load1 = buffer.Load4(read_offset); read_offset += 4*4;
				uint4 load2 = buffer.Load4(read_offset); read_offset += 4*4;
				uint4 load3 = buffer.Load4(read_offset); read_offset += 4*4;

				ui_command_box_t box = decode_ui_box(load0, load1, load2, load3);

				float d = sdf_box_rounded(box.origin, box.radius, box.roundedness);
				sdf = min(sdf, d);
			} break;

			case UiCommand_image:
			{
			} break;
		}
	}
}
