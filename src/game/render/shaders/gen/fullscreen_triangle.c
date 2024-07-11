// Generated by metagen.lua

#define DF_SHADER_FULLSCREEN_TRIANGLE_SOURCE_CODE \
	"// Generated by metagen.lua\n" \
	"\n" \
	"#include \"bindless.hlsli\"\n" \
	"\n" \
	"\n" \
	"\n" \
	"#include \"common.hlsli\"\n" \
	"\n" \
	"FullscreenTriangleOutVS MainVS(uint id : SV_VertexID)\n" \
	"{\n" \
	"	FullscreenTriangleOutVS OUT;\n" \
	"	OUT.uv  = uint2(id, id << 1) & 2;\n" \
	"	OUT.pos = float4(lerp(float2(-1, 1), float2(1, -1), OUT.uv), 0, 1);\n" \
	"	return OUT;\n" \
	"}\n" \
	"\n" \