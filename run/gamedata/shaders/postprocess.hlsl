#include "common.hlsl"

// -----------------------------------------------------------------------

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

PS_INPUT postprocess_vs(uint id : SV_VertexID)
{
    PS_INPUT OUT;
    OUT.uv  = uint2(id, id << 1) & 2;
    OUT.pos = float4(lerp(float2(-1, 1), float2(1, -1), OUT.uv), 0, 1);
    return OUT;
}

// -----------------------------------------------------------------------
// MSAA Resolve (includes fog rendering)

Texture2DMS<float3> msaa_rendertarget : register(t0);
Texture2DMS<float>  depth_buffer      : register(t1);
Texture2D<float4>   blue_noise        : register(t2);
Texture3D           fogmap            : register(t3);

float3 iq_hash2(uint3 x)
{
    static const uint k = 1103515245U;  // GLIB C

    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    
    return float3(x)*(1.0/float(0xffffffffU));
}

float noise_3d(float3 co)
{
    uint3  i = uint3(co);
    float3 f =  frac(co);

    return lerp(lerp(lerp(iq_hash2(i + uint3(0, 0, 0)).x,
                          iq_hash2(i + uint3(1, 0, 0)).x, f.x),
                     lerp(iq_hash2(i + uint3(0, 1, 0)).x,
                          iq_hash2(i + uint3(1, 1, 0)).x, f.x), f.y),
                lerp(lerp(iq_hash2(i + uint3(0, 0, 1)).x,
                          iq_hash2(i + uint3(1, 0, 1)).x, f.x),
                     lerp(iq_hash2(i + uint3(0, 1, 1)).x,
                          iq_hash2(i + uint3(1, 1, 1)).x, f.x), f.y), f.z);
}

float3 fog_blend(float3 color, float4 fog)
{
    return color*fog.a + fog.rgb;
}

float3 sample_fog_lighting(float3 p)
{
    float3 sample_p = (p - fog_offset) / fog_dim + 0.5f;
    return fogmap.SampleLevel(sampler_fog, sample_p, 0).rgb;
}

float4 raymarch_fog(float2 uv, uint2 co, float dither, uint sample_index)
{
    float3 o, d;
    camera_ray(uv, o, d);

    float max_distance = 1024.0f;
    uint  steps        = 32;

    float t      = 0;
    float t_step = rcp(steps);
    float depth  = 1.0f / depth_buffer.Load(co, sample_index);

    float stop_distance = min(depth, max_distance);

    float density    = 0.01;
    float absorption = 0.002;
    float scattering = 0.01;
    float extinction = absorption + scattering;

    float3 ambient = 0; // 0.5*float3(0.15, 0.30, 0.62);

    float3 illumination = 0;
    float  transmission = 1;
    for (uint i = 0; i < steps; i++)
    {
        float step_size = stop_distance*t_step;

        float  dist = stop_distance*(t - dither*t_step);
        float3 p = o + dist*d;

        // float3 density_sample_p = p / 128.0 + (frame_index / 250.0);
        float local_density = density; //  + 0.11*(noise_3d(density_sample_p) - 0.5);

        transmission *= exp(-local_density*extinction*step_size);

        float3 direct_light = sample_fog_lighting(p);

        float3 in_scattering  = ambient + direct_light;
        float  out_scattering = scattering*local_density;

        float3 current_light = in_scattering*out_scattering;

        illumination += transmission*current_light*step_size;    
        t += t_step;
    }

    return float4(illumination, transmission);
}

float3 msaa_tonemap(float3 color)
{
    return color * (rcp(1.0 + max3(color)));
}

float3 msaa_tonemap_inverse(float3 color)
{
    return color * (rcp(1.0 - max3(color)));
}

float4 msaa_resolve_ps(PS_INPUT IN) : SV_TARGET
{
    uint2 dim; uint sample_count;
    msaa_rendertarget.GetDimensions(dim.x, dim.y, sample_count);

    uint2 co = IN.uv*dim;

    bool require_full_multisampled_raymarch = false;

    float3 color_samples[8];
    color_samples[0] = msaa_rendertarget.Load(co, 0);

    uint2 dither_dim;
    blue_noise.GetDimensions(dither_dim.x, dither_dim.y);
    
    float4 dither = blue_noise.Load(uint3(co % dither_dim, 0)); 
    dither = dither.xyzw - dither.yzwx;

    {for (uint i = 1; i < sample_count; i++)
    {
        color_samples[i] = msaa_rendertarget.Load(co, i);
        if (any(color_samples[i] != color_samples[i - 1]))
        {
            require_full_multisampled_raymarch = true;
        }
    }}

    float raymarch_dither = dither[frame_index % 4];

    float3 color = 0;
    if (require_full_multisampled_raymarch)
    {
        {for (uint i = 0; i < sample_count; i++)
        {
            float4 fog = raymarch_fog(IN.uv, co, raymarch_dither, i);;
            float3 sample_color = fog_blend(color_samples[i], fog);
            color += msaa_tonemap(sample_color);
        }}
    }
    else
    {
        float4 fog = raymarch_fog(IN.uv, co, raymarch_dither, 0);
        {for (uint i = 0; i < sample_count; i++)
        {
            float3 sample_color = fog_blend(color_samples[i], fog);
            color += msaa_tonemap(sample_color);
        }}
    }
    color *= rcp(sample_count);
    color = msaa_tonemap_inverse(color);

    return float4(color.rgb, 1);
}

// -----------------------------------------------------------------------
// Final post-effect shader

Texture2D<float3> hdr_rendertarget : register(t0);

float3 tonemap(float3 color)
{
    return 1.0 - exp(-color);
}

float4 hdr_resolve_ps(PS_INPUT IN) : SV_TARGET
{
    uint2 co = uint2(IN.pos.xy); 

    float3 color = hdr_rendertarget.Load(uint3(co, 0));

    float3 bloom = 0;
    bloom += hdr_rendertarget.SampleLevel(sampler_linear_clamped, IN.uv, 1);
    bloom += hdr_rendertarget.SampleLevel(sampler_linear_clamped, IN.uv, 2);
    bloom += hdr_rendertarget.SampleLevel(sampler_linear_clamped, IN.uv, 3);
    bloom += hdr_rendertarget.SampleLevel(sampler_linear_clamped, IN.uv, 4);
    bloom += hdr_rendertarget.SampleLevel(sampler_linear_clamped, IN.uv, 5);
    bloom /= 5.0;

    color = lerp(color, bloom, 0.1);

    uint2 dither_dim;
    blue_noise.GetDimensions(dither_dim.x, dither_dim.y);
    
    float4 dither = blue_noise.Load(uint3(co % dither_dim, 0)); 
    dither = dither.xyzw - dither.yzwx;

    color = tonemap(color);
    color = pow(color, 1.0 / 2.23);
    color += dither.rgb / 255.0;

    return float4(color, 1);
}
