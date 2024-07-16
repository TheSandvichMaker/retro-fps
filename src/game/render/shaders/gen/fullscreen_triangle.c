// Generated by metagen.lua

#define DF_SHADER_FULLSCREEN_TRIANGLE_SOURCE_CODE \
	"// Generated by metagen.lua\n" \
	"\n" \
	"\n" \
	"\n" \
	"#include \"common.hlsli\"\n" \
	"\n" \
	"fullscreen_triangle_vs_out_t fullscreen_triangle_vs(uint id : SV_VertexID)\n" \
	"{\n" \
	"	fullscreen_triangle_vs_out_t OUT;\n" \
	"	OUT.uv  = uint2(id, id << 1) & 2;\n" \
	"	OUT.pos = float4(lerp(float2(-1, 1), float2(1, -1), OUT.uv), 0, 1);\n" \
	"	return OUT;\n" \
	"}\n" \
	"\n" \
