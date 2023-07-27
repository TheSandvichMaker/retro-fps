#include "common.hlsl"

struct r_ui_rect_t
{
	float2 p_min;
	float2 p_max;
	float4 roundedness;
	uint   color;
	float  shadow_radius;
	float  shadow_amount;
	float  pad1;
};

StructuredBuffer<r_ui_rect_t> ui_rects : register(t0);

struct PS_INPUT
{
	float4 pos : SV_Position;
	uint   id  : INSTANCE_ID;
	float4 col : COLOR;
};

PS_INPUT vs(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	r_ui_rect_t rect = ui_rects[instance_id + instance_offset];

	float r = rect.shadow_radius + 1.0f;
	
	float4 verts[] = {
		{ rect.p_min.x - r, rect.p_min.y - r, 0, 1 },
		{ rect.p_min.x - r, rect.p_max.y + r, 0, 1 },
		{ rect.p_max.x + r, rect.p_min.y - r, 0, 1 },
		{ rect.p_max.x + r, rect.p_max.y + r, 0, 1 },
	};

	float4 vert = verts[vertex_id];
	vert = 2.0f*(vert / float4(screen_dim, 1, 1)) - 1.0f;

	PS_INPUT OUT;
	OUT.pos = vert;
	OUT.id  = instance_id + instance_offset;
	OUT.col = unpack_color(rect.color);
	return OUT;
}

float box(float2 p, float2 b)
{
	float2 d = abs(p) - b;
	return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}

float rounded_box(float2 p, float2 b, float4 r)
{
	r.xy = (p.x > 0.0f) ? r.xy : r.zw;
	r.x  = (p.y > 0.0f) ? r.x  : r.y;
	float2 q = abs(p) - b + r.x;
	return min(max(q.x, q.y), 0.0f) + length(max(q, 0.0f)) - r.x;
}

float4 ps(PS_INPUT IN) : SV_Target
{
	r_ui_rect_t rect = ui_rects[IN.id];

	float2 pos = IN.pos.xy;
	pos.y = screen_dim.y - pos.y;

	float2 p = 0.5f*(rect.p_min + rect.p_max);
	float2 b = 0.5f*(rect.p_max - rect.p_min);

	float d = rounded_box(pos - p, b, rect.roundedness);

	float shadow_radius = rect.shadow_radius;

	float aa_radius = max(fwidth(pos.x), fwidth(pos.y));
	float aa  = smoothstep(0.5f*aa_radius, -0.5f*aa_radius, d);
	float rim = 1.0f; // smoothstep(-1.0f*aa_radius, -3.0f*aa_radius, d);

	float4 color = IN.col;
	color.rgb = lerp(color.rgb, 1.5f*color.rgb, 1.0f - rim);
	color.a *= aa;

	// float4 shadow = float4(0, 0, 0, rect.shadow_amount*(1.0f - saturate(d / shadow_radius)));
	// shadow.a *= shadow.a;

	// float4 result = lerp(shadow, color, saturate(aa));
	// result.rgb *= result.a;
	float4 result = color;

	return result;
}
