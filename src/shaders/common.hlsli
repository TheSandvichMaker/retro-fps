#pragma once

#include "bindless.hlsli"
#include "gen/view.hlsli"
 
// TODO: Remove
ConstantBuffer< view_parameters_t > view : register(b2);

#define PI (3.1415926535898)

struct quad_vs_out_t
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct fullscreen_triangle_vs_out_t
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

void camera_ray(float2 uv, out float3 position, out float3 direction)
{
    float3 camera_x = {
        view.world_to_view[0][0],
        view.world_to_view[0][1],
        view.world_to_view[0][2],
    };

    float3 camera_y = {
        view.world_to_view[1][0],
        view.world_to_view[1][1],
        view.world_to_view[1][2],
    };

    float3 camera_z = {
        view.world_to_view[2][0],
        view.world_to_view[2][1],
        view.world_to_view[2][2],
    };

    position = -(view.world_to_view[0][3]*camera_x +
                 view.world_to_view[1][3]*camera_y +
                 view.world_to_view[2][3]*camera_z);

    float2 nds = 2*uv - 1;
    nds.y = -nds.y;

    float film_half_w = rcp(view.view_to_clip[0][0]);
    float film_half_h = rcp(view.view_to_clip[1][1]);

    direction = normalize(nds.x*camera_x*film_half_w +
                          nds.y*camera_y*film_half_h -
                          camera_z);
}

float square(float x)
{
	return x*x;
}

float Max3(float3 v)
{
	return max(v.x, max(v.y, v.z));
}

float QuasirandomDither(float2 co)
{
	const float2 magic = float2(0.75487766624669276, 0.569840290998);
    return frac(dot(co, magic));
}

template <typename T>
T RemapTriPDF(T n)
{
    T orig = n * 2.0 - 1.0;
    n = orig * rsqrt(abs(orig));
    return max(-1.0, n) - sign(orig);
}

struct ColorRGBA8
{
	uint color;
};

float4 Unpack(ColorRGBA8 rgba8)
{
	uint color = rgba8.color;

	float4 result = float4((float)((color >>  0) & 0xFF) / 255.0f,
						   (float)((color >>  8) & 0xFF) / 255.0f,
						   (float)((color >> 16) & 0xFF) / 255.0f,
						   (float)((color >> 24) & 0xFF) / 255.0f);
	return result;
}

float LinearToSRGB(float lin)
{
	return lin <= 0.0031308 ? 12.92*lin : 1.055*pow(lin, 1.0 / 2.4) - 0.055;
}

float3 LinearToSRGB(float3 lin)
{
	return select(lin <= 0.0031308, 12.92*lin, 1.055*pow(lin, 1.0 / 2.4) - 0.055);
}

float3 SRGBToLinear(float3 srgb)
{
	return select(srgb <= 0.04045, srgb / 12.92, pow((srgb + 0.055) / 1.055, 2.4));
}

float4 LinearToSRGB(float4 lin)
{
	return float4(LinearToSRGB(lin.rgb), lin.a);
}

float4 SRGBToLinear(float4 srgb)
{
	return float4(SRGBToLinear(srgb.rgb), srgb.a);
}

float2 FatPixel(float2 texture_dim, float2 in_uv)
{
	float2 texel_coord = in_uv * texture_dim;

	float2 uv = floor(texel_coord) + 0.5;
	uv += 1.0 - saturate((1.0 - frac(texel_coord)) / fwidth(in_uv));
	uv *= rcp(texture_dim);

	return uv;
}

float SampleShadowPCF3x3(Texture2D<float> shadowmap, float2 shadowmap_dim, float2 projected_pos, float test_depth)
{
    float4 sfrac;
    sfrac.xy = frac(projected_pos*shadowmap_dim - 0.5);
    sfrac.zw = 1.0 - sfrac.xy;

    float4 gather4_a = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2(-1, -1));
    float4 gather4_b = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2( 1, -1));
    float4 gather4_c = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2(-1,  1));
    float4 gather4_d = shadowmap.GatherRed(df::s_aniso_clamped, projected_pos, int2( 1,  1));

    float transmittance = 0.0;
    transmittance += dot(float4(sfrac.z, 1, sfrac.w, sfrac.z*sfrac.w), test_depth >= gather4_a);
    transmittance += dot(float4(1, sfrac.x, sfrac.x*sfrac.w, sfrac.w), test_depth >= gather4_b);
    transmittance += dot(float4(sfrac.z*sfrac.y, sfrac.y, 1, sfrac.z), test_depth >= gather4_c);
    transmittance += dot(float4(sfrac.y, sfrac.x*sfrac.y, sfrac.x, 1), test_depth >= gather4_d);
    transmittance /= 9.0;

    return transmittance;
}

float SampleSunShadow(Texture2D<float> sun_shadowmap, float3 world_p)
{
    float3 projected_p = mul(view.sun_matrix, float4(world_p, 1)).xyz;
    projected_p.y  = -projected_p.y;
    projected_p.xy = 0.5*projected_p.xy + 0.5;

    float p_depth = max(0.0, projected_p.z);

    float sun_shadow = 1.0f;
    {
        float2 dim;
        sun_shadowmap.GetDimensions(dim.x, dim.y);

        sun_shadow = SampleShadowPCF3x3(sun_shadowmap, dim, projected_p.xy, p_depth);
    }

    return sun_shadow;
}

float2 SvPositionToClip(float2 sv_position)
{ 
	return 2.0f*(sv_position / view.view_size) - 1.0f;
}

float3 oklab_from_linear_srgb(float3 c) 
{
    float l = 0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z;
	float m = 0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z;
	float s = 0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z;

    float l_ = pow(l, 1.0 / 3.0);
    float m_ = pow(m, 1.0 / 3.0);
    float s_ = pow(s, 1.0 / 3.0);

    float3 result = {
        0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_,
        1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_,
        0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_,
    };

	return result;
}

float3 linear_srgb_from_oklab(float3 c) 
{
    float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
    float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
    float s_ = c.x - 0.0894841775f * c.y - 1.2914855480f * c.z;

    float l = l_*l_*l_;
    float m = m_*m_*m_;
    float s = s_*s_*s_;

    float3 result = {
		+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
		-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
		-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    };

	return result;
}

float3 oklch_from_oklab(float3 c)
{
	float3 result = {
		c.x,
		sqrt(c.y*c.y + c.z*c.z),
		atan2(c.z, c.y),
	};
	return result;
}

float3 oklab_from_oklch(float3 c)
{
	float3 result = {
		c.x,
		c.y*cos(c.z),
		c.y*sin(c.z),
	};
	return result;
}

float3 oklch_from_linear_srgb(float3 c)
{
	return oklch_from_oklab(oklab_from_linear_srgb(c));
}

float3 linear_srgb_from_oklch(float3 c)
{
	return linear_srgb_from_oklab(oklab_from_oklch(c));
}

float3 rgb_from_hsv(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html
float3 ColorMapTurbo(float t)
{
    const float3 c0 = float3(0.1140890109226559, 0.06288340699912215, 0.2248337216805064);
    const float3 c1 = float3(6.716419496985708, 3.182286745507602, 7.571581586103393);
    const float3 c2 = float3(-66.09402360453038, -4.9279827041226, -10.09439367561635);
    const float3 c3 = float3(228.7660791526501, 25.04986699771073, -91.54105330182436);
    const float3 c4 = float3(-334.8351565777451, -69.31749712757485, 288.5858850615712);
    const float3 c5 = float3(218.7637218434795, 67.52150567819112, -305.2045772184957);
    const float3 c6 = float3(-52.88903478218835, -21.54527364654712, 110.5174647748972);

    t = saturate(t);
    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));
}
