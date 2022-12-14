#include "common.hlsl"

#define USE_DYNAMIC_SUN_SHADOWS 1

struct PS_INPUT
{
    float4 pos           : SV_POSITION;
    float4 shadowmap_pos : SHADOWMAP_POSITION;
    float2 uv            : TEXCOORD;
    float2 uv_lightmap   : TEXCOORD_LIGHTMAP;
    float3 normal        : NORMAL;
};

Texture2D        albedo    : register(t0);
Texture2D        lightmap  : register(t1);
Texture2D<float> shadowmap : register(t2);

PS_INPUT vs(VS_INPUT_BRUSH IN)
{
    PS_INPUT OUT;
    OUT.pos           = mul(mul(mul(proj_matrix, view_matrix), model_matrix), float4(IN.pos, 1));
    OUT.shadowmap_pos = mul(mul(sun_matrix, model_matrix), float4(IN.pos, 1));
    OUT.uv            = IN.uv;
    OUT.uv_lightmap   = IN.uv_lightmap;
    OUT.normal        = IN.normal;
    return OUT;
}

float interpolate_gather(float4 gather, float2 interp)
{
    return lerp(lerp(gather.w, gather.z, interp.x), 
                lerp(gather.x, gather.y, interp.x), interp.y);
}

float test_shadow(float4 shadow_pos, float3 normal)
{
    float3 projected_pos = shadow_pos.xyz / shadow_pos.w;
    projected_pos.y  = -projected_pos.y;
    projected_pos.xy = 0.5*projected_pos.xy + 0.5;

    float current_depth = projected_pos.z;

    if (current_depth < 0.0 || current_depth > 1.0)
    {
        return 0.0;
    }
    else
    {
        float bias = max(0.025*(1.0 - max(0, dot(normal, light_direction))), 0.010);
        float biased_depth = current_depth + bias;

        float shadow = sample_pcf_3x3(shadowmap, projected_pos.xy, current_depth, bias);
        return shadow;
    }
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    float2 dim;
    albedo.GetDimensions(dim.x, dim.y);

    float2 uv  = fat_pixel(dim, IN.uv);
    float3 tex = albedo.Sample(sampler_linear, uv).rgb;

    float3 lighting = pyramid_blur(lightmap, sampler_linear_clamped, IN.uv_lightmap).rgb;
    float3 normal   = normalize(mul(model_matrix, float4(IN.normal, 0))).xyz; // don't use non-uniform scaling!! 

#if USE_DYNAMIC_SUN_SHADOWS
    float shadow = test_shadow(IN.shadowmap_pos, normal);

    float ndotl = max(0, dot(normal, light_direction));
    lighting += sun_color*ndotl*(1.0 - shadow);
#endif

    float3 col = tex*lighting;

    return float4(col, 1);
}
