#include "common.hlsl"

#define R_UI_RECT_BLEND_TEXT (1 << 0)

struct r_ui_rect_t
{
	float2 p_min;           // 8
	float2 p_max;           // 16
	float2 uv_min;          // 24
	float2 uv_max;          // 32
	float4 roundedness;     // 48
	uint   color_00;        // 52
	uint   color_10;        // 58
	uint   color_11;        // 64
	uint   color_01;        // 68
	float  shadow_radius;   // 74
	float  shadow_amount;   // 80
	float  inner_radius;    // 84
	uint   flags;           // 88
	float  pad1;            // 92
	float  pad2;            // 96
};

StructuredBuffer<r_ui_rect_t> ui_rects : register(t0);
Texture2D                     texture0 : register(t1);

struct PS_INPUT
{
	float4 pos : SV_Position;
	uint   id  : INSTANCE_ID;
	float4 col : COLOR;
    float2 uv  : TEXCOORD;
};

PS_INPUT vs(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
	r_ui_rect_t rect = ui_rects[instance_id + instance_offset];

	float r = rect.shadow_radius + 1.0f;

    // float rect_w = rect.p_max.x - rect.p_min.y;
    // float rect_h = rect.p_max.y - rect.p_min.y;
    // float2 r_scaled = r / float2(rect_w, rect_h);
    float uv_w = rect.uv_max.x - rect.uv_min.x;
    float uv_h = rect.uv_max.y - rect.uv_min.y;
    float2 r_scaled = r*float2(uv_w, uv_h) / screen_dim;
	
	float4 verts[] = {
		{ rect.p_min.x - r, rect.p_min.y - r, 0, 1 },
		{ rect.p_min.x - r, rect.p_max.y + r, 0, 1 },
		{ rect.p_max.x + r, rect.p_min.y - r, 0, 1 },
		{ rect.p_max.x + r, rect.p_max.y + r, 0, 1 },
	};

    float2 uvs[] = {
        { rect.uv_min.x - r_scaled.x, rect.uv_min.y - r_scaled.y },
        { rect.uv_min.x - r_scaled.x, rect.uv_max.y + r_scaled.y },
        { rect.uv_max.x + r_scaled.x, rect.uv_min.y - r_scaled.y },
        { rect.uv_max.x + r_scaled.x, rect.uv_max.y + r_scaled.y },
    };

	uint colors[] = {
		rect.color_00,
		rect.color_01,
		rect.color_10,
		rect.color_11,
	};

	float4 vert = verts[vertex_id];
	vert = 2.0f*(vert / float4(screen_dim, 1, 1)) - 1.0f;

	PS_INPUT OUT;
	OUT.pos = vert;
    OUT.uv  = uvs[vertex_id]; // TODO: Scale to counteract distortion from shadow radius
	OUT.id  = instance_id + instance_offset;
	OUT.col = unpack_color(colors[vertex_id]);
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
	return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;
}

float r_dither(float2 co)
{
	const float2 magic = float2(0.75487766624669276, 0.569840290998);
    return frac(dot(co, magic));
}

float remap_tri(float n)
{
    float orig = n * 2.0 - 1.0;
    n = orig * rsqrt(abs(orig));
    return max(-1.0, n) - sign(orig);
}

float4 ps(PS_INPUT IN) : SV_Target
{
	r_ui_rect_t rect = ui_rects[IN.id];

	float2 pos = IN.pos.xy;
	pos.y = screen_dim.y - pos.y - 1;

	float2 p = 0.5f*(rect.p_min + rect.p_max);
	float2 b = 0.5f*(rect.p_max - rect.p_min);

	float d = rounded_box(pos - p, b, rect.roundedness);

	float aa = smoothstep(0.5f, -0.5f, d);

	if (d < 0.0f && rect.inner_radius > 0.0f)
	{
		aa *= smoothstep(rect.inner_radius + 0.5f, rect.inner_radius - 0.5f, -d);
	}

	float4 color = 0;
	if (aa > 0.0f)
	{
		color = IN.col;
	}

    float4 tex_col = texture0.SampleLevel(sampler_linear_clamped, IN.uv, 0);
    if (rect.flags & R_UI_RECT_BLEND_TEXT)
    {
        color *= sqrt(tex_col.r);
    }
    else
    {
        color *= tex_col;
        color.a *= aa;
    }

	float shadow_radius = rect.shadow_radius;
	float shadow = min(1.0, exp(-5.0f*d / shadow_radius)); // 1.0f - saturate(d / shadow_radius);
	shadow *= rect.shadow_amount;

	color += float4(0, 0, 0, (1.0f - aa)*shadow);
    color += remap_tri(r_dither(IN.pos.xy)) / 255.0f;

	return color;
}
