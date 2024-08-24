// Generated by metagen.lua from resolve.metashader

#include "common.hlsli"

#include "gen/resolve.hlsli"

ConstantBuffer< resolve_draw_parameters_t > draw : register(b0);

float3 fog_blend(float3 color, float4 fog)
{
    return color*fog.a + fog.rgb;
}

float henyey_greenstein(float3 v, float3 l, float k)
{
    float numerator = (1.0 - k*k);
    float denominator = square(1.0 - k*dot(l, v));
    return (1.0 / (4*PI))*(numerator / denominator);
}

float3 sample_fog_map(float3 p)
{
	Texture3D<float4> fogmap = draw.fogmap.Get();

    float3 sample_p = (p - view.fog_offset) / view.fog_dim + 0.5f;
    return fogmap.SampleLevel(df::s_linear_border, sample_p, 0).rgb;
}

float4 integrate_fog(float2 uv, uint2 co, float dither, int sample_index)
{
    float density = view.fog_density;

	if (density <= 0.0)
	{
		return float4(0, 0, 0, 1);
	}

	float3 o, d;
	camera_ray(uv, o, d);

	float max_march_distance = 1024.0;
	int   steps              = 1;

	float t_step = 1.0 / float(steps);
	float depth  = 1.0 / draw.depth_buffer.Get().Load(co, sample_index);
	float t      = dither*t_step;

	float stop_distance = min(depth, max_march_distance);

    float absorption = view.fog_absorption;
    float scattering = view.fog_scattering;
    float extinction = absorption + scattering;
    float phase_k    = view.fog_phase_k;
    float sun_phase  = henyey_greenstein(d, view.sun_direction, phase_k);

    float3 ambient = view.fog_ambient_inscattering;

    float3 illumination = 0.0;
    float  transmission = 1.0;
    for (int i = 0; i < steps; i++)
    {
        float step_size = stop_distance*t_step;

        float  dist = stop_distance*t;
        float3 p    = o + dist*d;

        transmission *= exp(-density*extinction*step_size);

        float3 direct_light = rcp(4.0f*PI)*(sample_fog_map(p) + ambient);

        float sun_shadow = SampleSunShadow(draw.shadow_map.Get(), p);
        direct_light += view.sun_color*sun_shadow*sun_phase;

        float3 in_scattering  = direct_light;
        float  out_scattering = scattering*density;

        float3 current_light = in_scattering*out_scattering;

        illumination += transmission*current_light*step_size;    
        t += t_step;
    }

    float remainder = depth - stop_distance;
    if (isinf(remainder))
    {
        transmission  = 0.0;
        illumination += view.sun_color*sun_phase*scattering*density*rcp(density*extinction);
    }
    else
    {
        transmission *= exp(-remainder*density*extinction);
        illumination += transmission*view.sun_color*sun_phase*scattering*density*remainder;
    }
    
    return float4(illumination, transmission);
}

float3 reversible_tonemap(float3 color)
{
	return color*rcp(1 + max(color.r, max(color.g, color.b)));
}

float3 reversible_tonemap_inverse(float3 color)
{
	return color*rcp(1 - max(color.r, max(color.g, color.b)));
}

float4 resolve_msaa_ps(fullscreen_triangle_vs_out_t IN) : SV_Target
{
	float2 uv = IN.uv;
	uint2  co = uint2(IN.pos.xy);

	Texture2DMS<float3> tex_color      = draw.hdr_color .Get();
	Texture2D  <float4> tex_blue_noise = draw.blue_noise.Get();

	float4 blue_noise = tex_blue_noise.Load(uint3(co % 64, 0));

    const float phi = 0.5*(1.0 + sqrt(5));

	float4 sum = 0.0;
	for (int i = 0; i < draw.sample_count; i++)
	{
		float3 color = tex_color.Load(co, i).rgb;

        float dither = frac(blue_noise[i % 4] + phi*float(i / 4));

		float4 fog = integrate_fog(IN.uv, co, blue_noise.a, i);
		color = fog_blend(color, fog);

		// float2 sample_position = tex_color.GetSamplePosition(i);
		float  weight          = 1.0;

		// tonemap
		color = reversible_tonemap(color.rgb);
		color *= weight;

		sum.rgb += color.rgb;
		sum.a   += weight;
	}

	sum.rgb *= rcp(sum.a);
	sum.rgb  = reversible_tonemap_inverse(sum.rgb);

	return float4(sum.rgb, 1.0);
}
