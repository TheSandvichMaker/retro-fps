#include "common.hlsl"

struct PS_INPUT
{
    float4 pos          : SV_POSITION;
	float3 pos_world    : POSITION_WORLD;
    float2 uv           : TEXCOORD;
    float4 col          : COLOR;
	float3 normal_world : NORMAL_WORLD;
};

Texture2D texture0 : register(t0);

PS_INPUT vs(VS_INPUT_IMMEDIATE IN)
{
	float4x4 mvp = mul(proj_matrix, view_matrix);

    PS_INPUT OUT;
    OUT.pos          = mul(mvp, float4(IN.pos, 1));
    OUT.pos.z       += depth_bias;
	OUT.pos_world    = IN.pos;
    OUT.uv           = IN.uv;
	OUT.normal_world = IN.normal;

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

	float lighting = saturate(dot(normalize(camera_p - IN.pos_world), normalize(IN.normal_world)));

    float4 result = IN.col*tex*lighting;
    return result;
}
