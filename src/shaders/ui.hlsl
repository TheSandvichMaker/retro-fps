#include "common.hlsli"

#define R_UI_RECT_BLEND_TEXT     (1 << 0)
#define R_UI_RECT_HUE_PICKER     (1 << 1)
#define R_UI_RECT_SAT_VAL_PICKER (1 << 2)

struct Rect2
{
	float2 min;
	float2 max;
};

bool PointInRect(Rect2 rect, float2 p)
{
	return all(p >= rect.min) && all(p <= rect.max);
}

struct Rect2Fixed
{
	uint data0;
	uint data1;
};

Rect2 Decode(Rect2Fixed fixed)
{
	Rect2 result;
	result.min.x = float((fixed.data0 >>  0) & 0xFFFF);// / 4.0;
	result.min.y = float((fixed.data0 >> 16) & 0xFFFF);// / 4.0;
	result.max.x = float((fixed.data1 >>  0) & 0xFFFF);// / 4.0;
	result.max.y = float((fixed.data1 >> 16) & 0xFFFF);// / 4.0;
	return result;
}

struct UIRect
{
	float2                    p_min;
	float2                    p_max;
	float2                    uv_min;
	float2                    uv_max;
	float4                    roundedness;
	ColorRGBA8                colors[4];
	float                     shadow_radius;
	float                     shadow_amount;
	float                     inner_radius;
	uint                      flags;
	Rect2Fixed                clip_rect;
	df::Resource< Texture2D > texture;
};

struct DrawParameters
{
	df::Resource< StructuredBuffer<UIRect> > rects;
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
			 uint instance_id     : SV_InstanceID)
{
	const UIRect rect = draw.rects.Get()[instance_id];

	const float r = rect.shadow_radius + 1.0f;

	const float2 center       = 0.5*(rect.p_min + rect.p_max);
	const float2 radius       = 0.5*(rect.p_max - rect.p_min);
	const float2 grown_radius = radius + r;
	
	const float2 uv_center       = 0.5*(rect.uv_min + rect.uv_max);
	const float2 uv_radius       = 0.5*(rect.uv_max - rect.uv_min);
	const float2 grown_uv_radius = (grown_radius / radius)*uv_radius;

	const float2 signs = {
		(vertex_id & 0x1) ? 1.0 : -1.0,
		(vertex_id & 0x2) ? 1.0 : -1.0,
	};

	const float2 vert = (center    + signs*grown_radius);
	const float2 uv   = (uv_center + signs*grown_uv_radius);
	const float2 pos  = SvPositionToClip(vert);

	const float4 color = SRGBToLinear(Unpack(rect.colors[vertex_id]));

	VSOut OUT;
	OUT.pos   = float4(pos, 0, 1);
    OUT.uv    = uv;
	OUT.id    = instance_id;
	OUT.color = color;
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

	Rect2 clip_rect = Decode(rect.clip_rect);

	float2 pos = IN.pos.xy;
	pos.y = view.view_size.y - pos.y - 1;

	if (!PointInRect(clip_rect, pos))
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

	// guard against inverted rects
	float2 uv_min = min(rect.uv_min, rect.uv_max);
	float2 uv_max = max(rect.uv_min, rect.uv_max);

	float2 uv = IN.uv;

	float4 color = IN.color;

	if (rect.flags & R_UI_RECT_HUE_PICKER)
	{
		float h = uv.y;
		color.rgb = rgb_from_hsv(float3(h, 1.0f, 1.0f));
		color.rgb = SRGBToLinear(color.rgb);
	}
	else if (rect.flags & R_UI_RECT_SAT_VAL_PICKER)
	{
		float s = uv.x;
		float v = uv.y;
		color.rgb = rgb_from_hsv(float3(LinearToSRGB(color.r), s, v));
		color.rgb = SRGBToLinear(color.rgb);
	}

	if (all(uv >= uv_min) && all(uv <= uv_max))
	{
		float4 tex_col = rect.texture.Get().SampleLevel(df::s_linear_clamped, uv, 0);
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
	color.rgb = LinearToSRGB(color.rgb);
    color += RemapTriPDF(QuasirandomDither(IN.pos.xy)) / 255.0f;
	color.rgb = SRGBToLinear(color.rgb);

	return color;
}
