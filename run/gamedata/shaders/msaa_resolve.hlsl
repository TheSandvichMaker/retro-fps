#include "common.hlsl"

    // float4x4 camera_projection;

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

Texture2DMS<float3> rendertarget : register(t0);
Texture3D           fogmap       : register(t1);
Texture2DMS<float>  depth_buffer : register(t2);

PS_INPUT vs(uint id : SV_VertexID)
{
    PS_INPUT OUT;
    OUT.uv  = uint2(id, id << 1) & 2;
    OUT.pos = float4(lerp(float2(-1, 1), float2(1, -1), OUT.uv), 0, 1);
    return OUT;
}

float4 hash43n(float3 p)
{
    p  = frac(p * float3(5.3987, 5.4421, 6.9371));
    p += dot(p.yzx, p.xyz  + float3(21.5351, 14.3137, 15.3247));
    return frac(float4(p.x * p.y * 95.4307, p.x * p.y * 97.5901, p.x * p.z * 93.8369, p.y * p.z * 91.6931 ));
}

float3 iq_hash2(uint3 x)
{
    static const uint k = 1103515245U;  // GLIB C

    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    
    return float3(x)*(1.0/float(0xffffffffU));
}

float3 raymarch_fog(float2 uv, uint2 co, float dither, uint sample_index)
{
    float3 o, d;
    camera_ray(uv, o, d);

    uint steps = 32;

    float t      = 0;
    float t_step = rcp(steps);

    float3 result = 0;
    float depth = 1.0f / depth_buffer.Load(co, sample_index);

    float3 fogmap_dim;
    fogmap.GetDimensions(fogmap_dim.x, fogmap_dim.y, fogmap_dim.z);

    float3 fogmap_vox_scale = 1.0f / fogmap_dim;

    for (uint i = 0; i < steps; i++)
    {
        float  t_ = 512.0f*(t - dither*t_step);
        float3 p = o + t_*d;

        bool ok = t_ < depth;
        if (!ok)
            break;

        float3 sample_p = (p - fog_offset) / fog_dim + 0.5f;
        result += fogmap.SampleLevel(sampler_fog, sample_p, 0).rgb;

        t += t_step;
    }

    result *= rcp(steps);

    return result;
}

float3 tonemap(float3 color)
{
    return 1 - exp(-color);
}

float4 ps(PS_INPUT IN) : SV_TARGET
{
    uint2 dim; uint sample_count;
    rendertarget.GetDimensions(dim.x, dim.y, sample_count);

    uint2 co = IN.uv*dim;

    bool require_full_multisampled_raymarch = false;

    float3 color_samples[8];
    color_samples[0] = rendertarget.Load(co, 0);

    float3 dither = iq_hash2(uint3(co, frame_index)).xyz;
    dither = dither.xyz - dither.zxy;

    {for (uint i = 1; i < sample_count; i++)
    {
        color_samples[i] = rendertarget.Load(co, i);
        if (any(color_samples[i] != color_samples[i - 1]))
        {
            require_full_multisampled_raymarch = true;
        }
    }}

    float3 color = 0;
    if (require_full_multisampled_raymarch)
    {
        {for (uint i = 0; i < sample_count; i++)
        {
            float3 sample_color = color_samples[i] + raymarch_fog(IN.uv, co, dither.x, i);
            color += tonemap(sample_color);
        }}
    }
    else
    {
        float3 fog = raymarch_fog(IN.uv, co, dither.x, 0);
        {for (uint i = 0; i < sample_count; i++)
        {
            float3 sample_color = color_samples[i] + fog;
            color += tonemap(sample_color);
        }}
    }
    color *= rcp(sample_count);

    color = pow(abs(color), 1.0 / 2.23);
    color += dither / 255.0;

    return float4(color.rgb, 1);
}
