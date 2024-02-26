#include "common.hlsl"

float4 shadowmap_vs(float3 in_pos : POSITION) : SV_POSITION
{
    float4 pos = mul(mul(sun_matrix, model_matrix), float4(in_pos, 1));
    return pos;
}
