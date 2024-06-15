#pragma once

#include "bindless.hlsli"
#include "gen/view.hlsli"

float Max3(float3 v)
{
	return max(v.x, max(v.y, v.z));
}

float QuasirandomDither(float2 co)
{
	const float2 magic = float2(0.75487766624669276, 0.569840290998);
    return frac(dot(co, magic));
}

float RemapTriPDF(float n)
{
    float orig = n * 2.0 - 1.0;
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

    float shadow = 0;
    shadow += dot(float4(sfrac.z, 1, sfrac.w, sfrac.z*sfrac.w), gather4_a < test_depth);
    shadow += dot(float4(1, sfrac.x, sfrac.x*sfrac.w, sfrac.w), gather4_b < test_depth);
    shadow += dot(float4(sfrac.z*sfrac.y, sfrac.y, 1, sfrac.z), gather4_c < test_depth);
    shadow += dot(float4(sfrac.y, sfrac.x*sfrac.y, sfrac.x, 1), gather4_d < test_depth);
    shadow /= 9.0;

    return shadow;
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
