// Generated by metagen.lua

void shader_ui_set_draw_params(rhi_command_list_t *list, ui_draw_parameters_t *params)
{
	rhi_validate_buffer_srv(params->rects, S("ui_draw_parameters_t::rects"));
	rhi_set_parameters(list, 0, params, sizeof(*params));
}

#define DF_SHADER_UI_SOURCE_CODE \
	"// Generated by metagen.lua\n" \
	"\n" \
	"#include \"bindless.hlsli\"\n" \
	"\n" \
	"#include \"common.hlsli\"\n" \
	"\n" \
	"#define R_UI_RECT_BLEND_TEXT     (1 << 0)\n" \
	"#define R_UI_RECT_HUE_PICKER     (1 << 1)\n" \
	"#define R_UI_RECT_SAT_VAL_PICKER (1 << 2)\n" \
	"\n" \
	"struct Rect2\n" \
	"{\n" \
	"	float2 min;\n" \
	"	float2 max;\n" \
	"};\n" \
	"\n" \
	"bool PointInRect(Rect2 rect, float2 p)\n" \
	"{\n" \
	"	return all(p >= rect.min) && all(p <= rect.max);\n" \
	"}\n" \
	"\n" \
	"struct Rect2Fixed\n" \
	"{\n" \
	"	uint data0;\n" \
	"	uint data1;\n" \
	"};\n" \
	"\n" \
	"Rect2 Decode(Rect2Fixed fixed)\n" \
	"{\n" \
	"	Rect2 result;\n" \
	"	result.min.x = float((fixed.data0 >>  0) & 0xFFFF);// / 4.0;\n" \
	"	result.min.y = float((fixed.data0 >> 16) & 0xFFFF);// / 4.0;\n" \
	"	result.max.x = float((fixed.data1 >>  0) & 0xFFFF);// / 4.0;\n" \
	"	result.max.y = float((fixed.data1 >> 16) & 0xFFFF);// / 4.0;\n" \
	"	return result;\n" \
	"}\n" \
	"\n" \
	"struct UIRect\n" \
	"{\n" \
	"	float2                    p_min;\n" \
	"	float2                    p_max;\n" \
	"	float2                    uv_min;\n" \
	"	float2                    uv_max;\n" \
	"	float4                    roundedness;\n" \
	"	ColorRGBA8                colors[4];\n" \
	"	float                     shadow_radius;\n" \
	"	float                     shadow_amount;\n" \
	"	float                     inner_radius;\n" \
	"	uint                      flags;\n" \
	"	Rect2Fixed                clip_rect;\n" \
	"	df::Resource< Texture2D > texture;\n" \
	"};\n" \
	"\n" \
	"struct ui_draw_parameters_t\n" \
	"{\n" \
	"	df::Resource< StructuredBuffer< UIRect > > rects;\n" \
	"};\n" \
	"\n" \
	"ConstantBuffer< ui_draw_parameters_t > draw : register(b0);\n" \
	"\n" \
	"\n" \
	"\n" \
	"struct VSOut\n" \
	"{\n" \
	"	float4 pos   : SV_Position;\n" \
	"	uint   id    : INSTANCE_ID;\n" \
	"	float4 color : COLOR;\n" \
	"	float2 uv    : UV;\n" \
	"};\n" \
	"\n" \
	"VSOut MainVS(\n" \
	"	uint vertex_id   : SV_VertexID, \n" \
	"	uint instance_id : SV_InstanceID)\n" \
	"{\n" \
	"	const UIRect rect = draw.rects.Get()[instance_id];\n" \
	"\n" \
	"	const float r = rect.shadow_radius + 1.0f;\n" \
	"\n" \
	"	const float2 center       = 0.5*(rect.p_min + rect.p_max);\n" \
	"	const float2 radius       = 0.5*(rect.p_max - rect.p_min);\n" \
	"	const float2 grown_radius = radius + r;\n" \
	"	\n" \
	"	const float2 uv_center       = 0.5*(rect.uv_min + rect.uv_max);\n" \
	"	const float2 uv_radius       = 0.5*(rect.uv_max - rect.uv_min);\n" \
	"	const float2 grown_uv_radius = (grown_radius / radius)*uv_radius;\n" \
	"\n" \
	"	const float2 signs = {\n" \
	"		(vertex_id & 0x1) ? 1.0 : -1.0,\n" \
	"		(vertex_id & 0x2) ? 1.0 : -1.0,\n" \
	"	};\n" \
	"\n" \
	"	const float2 vert = (center    + signs*grown_radius);\n" \
	"	const float2 uv   = (uv_center + signs*grown_uv_radius);\n" \
	"	const float2 pos  = SvPositionToClip(vert);\n" \
	"\n" \
	"	const float4 color = SRGBToLinear(Unpack(rect.colors[vertex_id]));\n" \
	"\n" \
	"	VSOut OUT;\n" \
	"	OUT.pos   = float4(pos, 0, 1);\n" \
	"	OUT.uv    = uv;\n" \
	"	OUT.id    = instance_id;\n" \
	"	OUT.color = color;\n" \
	"	return OUT;\n" \
	"}\n" \
	"\n" \
	"float Box(float2 p, float2 b)\n" \
	"{\n" \
	"	float2 d = abs(p) - b;\n" \
	"	return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);\n" \
	"}\n" \
	"\n" \
	"float RoundedBox(float2 p, float2 b, float4 r)\n" \
	"{\n" \
	"	r.xy = (p.x > 0.0f) ? r.xy : r.zw;\n" \
	"	r.x  = (p.y > 0.0f) ? r.x  : r.y;\n" \
	"	float2 q = abs(p) - b + r.x;\n" \
	"	return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;\n" \
	"}\n" \
	"\n" \
	"float4 MainPS(VSOut IN) : SV_Target\n" \
	"{\n" \
	"	UIRect rect = draw.rects.Get()[IN.id];\n" \
	"\n" \
	"	Rect2 clip_rect = Decode(rect.clip_rect);\n" \
	"\n" \
	"	float2 pos = IN.pos.xy;\n" \
	"	pos.y = view.view_size.y - pos.y - 1;\n" \
	"\n" \
	"	if (!PointInRect(clip_rect, pos))\n" \
	"	{\n" \
	"		discard;\n" \
	"	}\n" \
	"\n" \
	"	float2 p = 0.5f*(rect.p_min + rect.p_max);\n" \
	"	float2 b = 0.5f*(rect.p_max - rect.p_min);\n" \
	"\n" \
	"	float d = RoundedBox(pos - p, b, rect.roundedness);\n" \
	"\n" \
	"	float aa = smoothstep(0.5f, -0.5f, d);\n" \
	"\n" \
	"	if (d < 0.0f && rect.inner_radius > 0.0f)\n" \
	"	{\n" \
	"		aa *= smoothstep(rect.inner_radius + 0.5f, rect.inner_radius - 0.5f, -d);\n" \
	"	}\n" \
	"\n" \
	"	float shadow_radius = rect.shadow_radius;\n" \
	"	float shadow = min(1.0, exp(-5.0f*d / shadow_radius)); // 1.0f - saturate(d / shadow_radius);\n" \
	"	shadow *= rect.shadow_amount;\n" \
	"\n" \
	"	// guard against inverted rects\n" \
	"	float2 uv_min = min(rect.uv_min, rect.uv_max);\n" \
	"	float2 uv_max = max(rect.uv_min, rect.uv_max);\n" \
	"\n" \
	"	float2 uv = IN.uv;\n" \
	"\n" \
	"	float4 color = IN.color;\n" \
	"\n" \
	"	if (rect.flags & R_UI_RECT_HUE_PICKER)\n" \
	"	{\n" \
	"		float h = uv.y;\n" \
	"		color.rgb = rgb_from_hsv(float3(h, 1.0f, 1.0f));\n" \
	"		color.rgb = SRGBToLinear(color.rgb);\n" \
	"	}\n" \
	"	else if (rect.flags & R_UI_RECT_SAT_VAL_PICKER)\n" \
	"	{\n" \
	"		float s = uv.x;\n" \
	"		float v = uv.y;\n" \
	"		color.rgb = rgb_from_hsv(float3(LinearToSRGB(color.r), s, v));\n" \
	"		color.rgb = SRGBToLinear(color.rgb);\n" \
	"	}\n" \
	"\n" \
	"	if (all(uv >= uv_min) && all(uv <= uv_max))\n" \
	"	{\n" \
	"		float4 tex_col = rect.texture.Get().SampleLevel(df::s_linear_clamped, uv, 0);\n" \
	"		if (rect.flags & R_UI_RECT_BLEND_TEXT)\n" \
	"		{\n" \
	"			color *= sqrt(tex_col.r);\n" \
	"		}\n" \
	"		else\n" \
	"		{\n" \
	"			color *= tex_col;\n" \
	"			color.a *= aa;\n" \
	"		}\n" \
	"	}\n" \
	"	else\n" \
	"	{\n" \
	"		color = 0;\n" \
	"	}\n" \
	"\n" \
	"	color.a += shadow;\n" \
	"	color.rgb = LinearToSRGB(color.rgb);\n" \
	"	color += RemapTriPDF(QuasirandomDither(IN.pos.xy)) / 255.0f;\n" \
	"	color.rgb = SRGBToLinear(color.rgb);\n" \
	"\n" \
	"	return color;\n" \
	"}\n" \
	"\n" \
	"float4 UIHeatMapPS(VSOut IN) : SV_Target\n" \
	"{\n" \
	"	UIRect rect = draw.rects.Get()[IN.id];\n" \
	"\n" \
	"	Rect2 clip_rect = Decode(rect.clip_rect);\n" \
	"\n" \
	"	float2 pos = IN.pos.xy;\n" \
	"	pos.y = view.view_size.y - pos.y - 1;\n" \
	"\n" \
	"	if (!PointInRect(clip_rect, pos))\n" \
	"	{\n" \
	"		discard;\n" \
	"	}\n" \
	"\n" \
	"	return float4(1.0 / 255.0f, 0.0f, 0.0f, 1.0f);\n" \
	"}\n" \
	"\n" \