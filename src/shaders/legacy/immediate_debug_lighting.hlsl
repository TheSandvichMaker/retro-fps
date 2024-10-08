#include "common.hlsl"

#define DRAW_NORMALS 1

struct PS_INPUT
{
    float4 pos          : SV_POSITION;
	float3 pos_world    : POSITION_WORLD;
    float2 uv           : TEXCOORD;
    float4 col          : COLOR;
	float3 normal_world : NORMAL_WORLD;
};

Texture2D texture0 : register(t0);

// make model matrix part of immediate draw calls...

PS_INPUT vs(VS_INPUT_IMMEDIATE IN)
{
	float4x4 mvp = mul(mul(proj_matrix, view_matrix), model_matrix);

    PS_INPUT OUT;
    OUT.pos          = mul(mvp, float4(IN.pos, 1));
    OUT.pos.z       += depth_bias;
	OUT.pos_world    = IN.pos;
    OUT.uv           = IN.uv;
	OUT.normal_world = normalize(mul(model_matrix, float4(IN.normal, 0)).xyz); // NOTE: not handling non-uniform scale

    OUT.col = float4((float)((IN.col >>  0) & 0xFF) / 255.0f,
                     (float)((IN.col >>  8) & 0xFF) / 255.0f,
                     (float)((IN.col >> 16) & 0xFF) / 255.0f,
                     (float)((IN.col >> 24) & 0xFF) / 255.0f);

    return OUT;
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    float4 tex = texture0.Sample(sampler_point, IN.uv);

    float3 camera_x = {
        view_matrix[0][0],
        view_matrix[0][1],
        view_matrix[0][2],
    };

    float3 camera_y = {
        view_matrix[1][0],
        view_matrix[1][1],
        view_matrix[1][2],
    };

    float3 camera_z = {
        view_matrix[2][0],
        view_matrix[2][1],
        view_matrix[2][2],
    };

	float3 camera_p = -(view_matrix[0][3]*camera_x +
						view_matrix[1][3]*camera_y +
						view_matrix[2][3]*camera_z);

	float3 light_d = normalize(camera_p - IN.pos_world);
	float lighting = saturate(dot(light_d, normalize(IN.normal_world)));

    float4 result = IN.col*tex;
	result.rgb   *= lighting;

#if DRAW_NORMALS
    result.rgb *= 0.5f + 0.5f*(IN.normal_world);
#endif

    return result;
}
