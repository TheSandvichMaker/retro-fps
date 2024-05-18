#include "common.hlsli"

#define R_UI_RECT_BLEND_TEXT (1 << 0)

struct Rect2
{
	float2 min;
	float2 max;

	bool PointInRect(float2 p)
	{
		return all(p >= min) && all(p <= max);
	}
};

struct Rect2Fixed
{
	uint data0;
	uint data1;

	Rect2 Decode()
	{
		Rect2 result;
		result.min.x = float((data0 >>  0) & 0xFFFF);// / 4.0;
		result.min.y = float((data0 >> 16) & 0xFFFF);// / 4.0;
		result.max.x = float((data1 >>  0) & 0xFFFF);// / 4.0;
		result.max.y = float((data1 >> 16) & 0xFFFF);// / 4.0;
		return result;
	}
};

struct UIRect
{
	float2     p_min;
	float2     p_max;
	float2     uv_min;
	float2     uv_max;
	float4     roundedness;
	ColorRGBA8 color_00;
	ColorRGBA8 color_10;
	ColorRGBA8 color_11;
	ColorRGBA8 color_01;
	float      shadow_radius;
	float      shadow_amount;
	float      inner_radius;
	uint       flags;
	Rect2Fixed clip_rect;
	// uint       texture;
};

struct DrawParameters
{
	df::Resource< StructuredBuffer<UIRect> > rects;
	df::Resource< Texture2D >                texture;
};

ConstantBuffer<DrawParameters> draw : register(b0);

struct VSOut
{
	float4 pos   : SV_Position;
	uint   id    : INSTANCE_ID;
	float4 color : COLOR;
	float2 uv    : UV;
};

VSOut MainVS(uint vertex_id       : SV_VertexID, 
			 uint instance_offset : SV_StartInstanceLocation,
			 uint instance_id     : SV_InstanceID)
{
	UIRect rect = draw.rects.Get()[instance_offset + instance_id];

	float r = rect.shadow_radius + 1.0f;

	float2 center       = 0.5*(rect.p_min + rect.p_max);
	float2 radius       = 0.5*(rect.p_max - rect.p_min);
	float2 grown_radius = radius + r;
	float2 grown_ratio  = grown_radius / radius;
	
	float4 verts[] = {
		{ center + float2(-grown_radius.x, -grown_radius.y), 0, 1 },
		{ center + float2(-grown_radius.x, +grown_radius.y), 0, 1 },
		{ center + float2(+grown_radius.x, -grown_radius.y), 0, 1 },
		{ center + float2(+grown_radius.x, +grown_radius.y), 0, 1 },
	};

	float2 uv_center       = 0.5*(rect.uv_min + rect.uv_max);
	float2 uv_radius       = 0.5*(rect.uv_max - rect.uv_min);
	float2 grown_uv_radius = grown_ratio*uv_radius;

    float2 uvs[] = {
		uv_center + float2(-grown_uv_radius.x, -grown_uv_radius.y),
		uv_center + float2(-grown_uv_radius.x, +grown_uv_radius.y),
		uv_center + float2(+grown_uv_radius.x, -grown_uv_radius.y),
		uv_center + float2(+grown_uv_radius.x, +grown_uv_radius.y),
    };

	ColorRGBA8 colors[] = {
		rect.color_00,
		rect.color_01,
		rect.color_10,
		rect.color_11,
	};

	float4 vert = verts[vertex_id];
	vert = 2.0f*(vert / float4(view.view_size, 1, 1)) - 1.0f;

	VSOut OUT;
	OUT.pos   = vert;
    OUT.uv    = uvs[vertex_id];
	OUT.id    = instance_id + instance_offset;
	OUT.color = colors[vertex_id].Unpack();
	return OUT;
}

float Box(float2 p, float2 b)
{
	float2 d = abs(p) - b;
	return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}

float RoundedBox(float2 p, float2 b, float4 r)
{
	r.xy = (p.x > 0.0f) ? r.xy : r.zw;
	r.x  = (p.y > 0.0f) ? r.x  : r.y;
	float2 q = abs(p) - b + r.x;
	return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;
}

float4 MainPS(VSOut IN) : SV_Target
{
	UIRect rect = draw.rects.Get()[IN.id];

	Rect2 clip_rect = rect.clip_rect.Decode();

	float2 pos = IN.pos.xy;
	pos.y = view.view_size.y - pos.y - 1;

	if (!clip_rect.PointInRect(pos))
	{
		discard;
	}

	float2 p = 0.5f*(rect.p_min + rect.p_max);
	float2 b = 0.5f*(rect.p_max - rect.p_min);

	float d = RoundedBox(pos - p, b, rect.roundedness);

	float aa = smoothstep(0.5f, -0.5f, d);

	if (d < 0.0f && rect.inner_radius > 0.0f)
	{
		aa *= smoothstep(rect.inner_radius + 0.5f, rect.inner_radius - 0.5f, -d);
	}

	float shadow_radius = rect.shadow_radius;
	float shadow = min(1.0, exp(-5.0f*d / shadow_radius)); // 1.0f - saturate(d / shadow_radius);
	shadow *= rect.shadow_amount;

	float4 color = IN.color;

	// guard against inverted rects
	float2 uv_min = min(rect.uv_min, rect.uv_max);
	float2 uv_max = max(rect.uv_min, rect.uv_max);

	float2 uv = IN.uv;

	if (all(uv >= uv_min) && all(uv <= uv_max))
	{
		float4 tex_col = draw.texture.Get().SampleLevel(df::s_linear_clamped, uv, 0);
		if (rect.flags & R_UI_RECT_BLEND_TEXT)
		{
			color *= sqrt(tex_col.r);
		}
		else
		{
			color *= tex_col;
			color.a *= aa;
		}
	}
	else
	{
		color = 0;
	}

	color.a += shadow;
    color += RemapTriPDF(QuasirandomDither(IN.pos.xy)) / 255.0f;

	return color;
}