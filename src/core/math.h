// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#include <xmmintrin.h>
#include <float.h>
#include <math.h>

typedef __m128 v4sf;  // vector of 4 float (sse1)

// allows for row-major creation of matrices
#define make_m4x4(e00, e01, e02, e03, \
                  e10, e11, e12, e13, \
                  e20, e21, e22, e23, \
                  e30, e31, e32, e33) \
    (m4x4_t) { \
        e00, e10, e20, e30, \
        e01, e11, e21, e31, \
        e02, e12, e22, e32, \
        e03, e13, e23, e33, \
    }

#define PI32 3.1415926536f

fn_local float square(float x)
{
	return x*x;
}

fn_local float to_radians(float deg)
{
    return deg*(PI32 / 180.0f);
}

fn_local float to_degrees(float rad)
{
    return rad*(180.0f / PI32);
}

fn_local float sqrt_ss(float x)
{
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set1_ps(x)));
}

fn_local float rsqrt_ss(float x)
{
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set1_ps(x)));
}

fn_local float abs_ss(float x)
{
    return (x < 0 ? -x : x); // TODO: fast version? does the compiler spot this and optimize it?
}

fn_local float sign_of(float x)
{
    if (x >= 0.0f)
        return 1.0f;
    else
        return -1.0f;
}

// forward declarations of sse_mathfun.h
fn_local v4sf log_ps(v4sf x);
fn_local v4sf exp_ps(v4sf x);
fn_local v4sf sin_ps(v4sf x);
fn_local v4sf cos_ps(v4sf x);
fn_local void sincos_ps(v4sf x, v4sf *s, v4sf *c);

// #pragma optimize("t", on)

fn_local float log_ss(float x)
{
    return _mm_cvtss_f32(log_ps(_mm_set1_ps(x)));
}

fn_local float exp_ss(float x)
{
    return _mm_cvtss_f32(exp_ps(_mm_set1_ps(x)));
}

fn_local float sin_ss(float x)
{
    return _mm_cvtss_f32(sin_ps(_mm_set1_ps(x)));
}

fn_local float cos_ss(float x)
{
    return _mm_cvtss_f32(cos_ps(_mm_set1_ps(x)));
}

fn_local void sincos_ss(float x, float *s, float *c)
{
    v4sf s_ps, c_ps;
    sincos_ps(_mm_set1_ps(x), &s_ps, &c_ps);

    *s = _mm_cvtss_f32(s_ps);
    *c = _mm_cvtss_f32(c_ps);
}

fn_local float tan_ss(float x)
{
    float s, c;
    sincos_ss(x, &s, &c);

    return s / c;
}

fn_local float fmod_ss(float x, float y)
{
    int n = (int)(x / y);
    return x - n*y;
}

fn_local bool flt_is_nan(float x)
{
	return x != x;
}

fn_local bool v3_contains_nan(v3_t x)
{
	return flt_is_nan(x.x) || flt_is_nan(x.y) || flt_is_nan(x.z);
}

#ifdef NO_STD_LIB
#pragma function(logf)
inline float logf(float x) { return log_ss(x); }
#pragma function(expf)
inline float expf(float x) { return exp_ss(x); }
#pragma function(sinf)
inline float sinf(float x) { return sin_ss(x); }
#pragma function(cosf)
inline float cosf(float x) { return cos_ss(x); }
#pragma function(tanf)
inline float tanf(float x) { return tan_ss(x); }
#pragma function(sqrtf)
inline float sqrtf(float x) { return sqrt_ss(x); }
#pragma function(fmodf)
inline float fmodf(float x, float y) { return fmod_ss(x, y); }
#pragma function(fabsf)
inline float fabsf(float x) { return abs_ss(x); }
#endif

// float

fn_local float lanczos(float x, float a)
{
    float result = 0.0f;

    if (fabsf(x) < 0.0001f)
        result = 1.0f;
    else if (-a <= x && x < a)
        result = (a*sinf(PI32*x)*sinf((PI32*x) / a)) / (PI32*PI32*x*x);

    return result;
}

fn_local float snorm_from_s16(int16_t s)
{
    return (float)s / (float)INT16_MAX;
}

fn_local int16_t s16_from_snorm(float s)
{
    if (s < -1.0f) s = -1.0f;
    if (s >  1.0f) s =  1.0f;
    return (int16_t)(s*(float)INT16_MAX);
}

fn_local float flt_add(float l, float r) { return l + r; }
fn_local float flt_sub(float l, float r) { return l - r; }
fn_local float flt_mul(float l, float r) { return l * r; }
fn_local float flt_div(float l, float r) { return l / r; }
fn_local float flt_min(float a, float b) { return a < b ? a : b; }
fn_local float flt_max(float a, float b) { return a > b ? a : b; }
fn_local float flt_clamp(float x, float l, float h) { return x < l ? l : x > h ? h : x; }
fn_local float flt_saturate(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

fn_local bool flt_equal(float a, float b, float margin)
{
	float diff = abs_ss(a - b);
	return diff <= margin;
}

fn_local float lerp(float l, float r, float t)
{
    return l*(1.0f - t) + r*t;
}

fn_local float smoothstep(float x)
{
    float result = x*x*(3.0f - 2.0f*x);
    return result;
}

fn_local float smootherstep(float x)
{
    float result = x*x*x*(x*(x*6.0f - 15.0f) + 10.0f);
    return result;
}

//
// color stuff
//

#define COLORF_WHITE   make_v4(1, 1, 1, 1)
#define COLORF_BLACK   make_v4(0, 0, 0, 1)
#define COLORF_RED     make_v4(1, 0, 0, 1)
#define COLORF_ORANGE  make_v4(1, 0.5f, 0, 1)
#define COLORF_GREEN   make_v4(0, 1, 0, 1)
#define COLORF_BLUE    make_v4(0, 0, 1, 1)
#define COLORF_MAGENTA make_v4(1, 0, 1, 1)
#define COLORF_CYAN    make_v4(0, 1, 1, 1)
#define COLORF_YELLOW  make_v4(1, 1, 0, 1)

#define COLOR32_WHITE   pack_rgba(1, 1, 1, 1)
#define COLOR32_BLACK   pack_rgba(0, 0, 0, 1)
#define COLOR32_RED     pack_rgba(1, 0, 0, 1)
#define COLOR32_GREEN   pack_rgba(0, 1, 0, 1)
#define COLOR32_BLUE    pack_rgba(0, 0, 1, 1)
#define COLOR32_MAGENTA pack_rgba(1, 0, 1, 1)
#define COLOR32_CYAN    pack_rgba(0, 1, 1, 1)
#define COLOR32_YELLOW  pack_rgba(1, 1, 0, 1)

fn_local v4_t linear_to_srgb(v4_t color)
{
    // TODO: more accurate srgb transforms?
    color.xyz = (v3_t){
        .x = sqrt_ss(color.x),
        .y = sqrt_ss(color.y),
        .z = sqrt_ss(color.z),
    };
    return color;
}

fn_local v4_t srgb_to_linear(v4_t color)
{
    // TODO: more accurate srgb transforms?
    color.xyz = (v3_t){
        .x = color.x*color.x,
        .y = color.y*color.y,
        .z = color.z*color.z,
    };
    return color;
}

fn_local float luminance(v3_t color)
{
	return color.x*0.2125f + color.y*0.7154f + color.z*0.0721f;
}

fn_local uint32_t pack_color(v4_t color)
{
    color.x = CLAMP(color.x, 0.0f, 1.0f);
    color.y = CLAMP(color.y, 0.0f, 1.0f);
    color.z = CLAMP(color.z, 0.0f, 1.0f);
    color.w = CLAMP(color.w, 0.0f, 1.0f);

    // color = linear_to_srgb(color);

#if 0
    color.x *= color.w;
    color.y *= color.w;
    color.z *= color.w;
#endif

    uint32_t result = (((uint32_t)(255.0f*color.x) <<  0) |
                       ((uint32_t)(255.0f*color.y) <<  8) |
                       ((uint32_t)(255.0f*color.z) << 16) |
                       ((uint32_t)(255.0f*color.w) << 24));
    return result;
}

fn_local uint32_t pack_rgba(float r, float g, float b, float a)
{
    return pack_color((v4_t){r, g, b, a});
}

fn_local uint32_t pack_rgb(float r, float g, float b)
{
    return pack_color((v4_t){r, g, b, 1.0f});
}

fn_local v4_t unpack_color(uint32_t color)
{
    float rcp_255 = 1.0f / 255.0f;

    v4_t result;
    result.x = rcp_255*(float)((color >>  0) & 0xFF);
    result.y = rcp_255*(float)((color >>  8) & 0xFF);
    result.z = rcp_255*(float)((color >> 16) & 0xFF);
    result.w = rcp_255*(float)((color >> 24) & 0xFF);

    return result;
}

// https://registry.khronos.org/OpenGL/extensions/EXT/EXT_packed_float.txt

fn_local unsigned pack_float11(float value)
{
	// This does not handle NaN or infinity.

    if (value < 0.0f) 
        value = 0.0f;

    if (value > 65024.0f)
        value = 65024.0f;

    union
    {
        float    f;
        unsigned i;
    } u;

    u.f = value;

    int exponent = ((u.i >> 23) & 0xFF) - 127;
    int mantissa = u.i & MASK_BITS(22);

    return (((exponent + 15) << 6) | (mantissa >> (23 - 6))) & MASK_BITS(11);
}

fn_local unsigned pack_float10(float value)
{
	// This does not handle NaN or infinity.

    if (value < 0.0f) 
        value = 0.0f;

    if (value > 64512.0f)
        value = 64512.0f;

    union
    {
        float    f;
        unsigned i;
    } u;

    u.f = value;

    int exponent = ((u.i >> 23) & 0xFF) - 127;
    int mantissa = u.i & MASK_BITS(22);

    return (((exponent + 15) << 5) | (mantissa >> (23 - 5))) & MASK_BITS(10);
}

fn_local uint32_t pack_r11g11b10f(v3_t color)
{
    unsigned r = pack_float11(color.x);
    unsigned g = pack_float11(color.y);
    unsigned b = pack_float10(color.z);

    unsigned result = (b << 22) | (g << 11) | r;

    return result;
}

//
// v2i_t
//

fn_local bool v2i_are_equal(v2i_t a, v2i_t b)
{
    return a.x == b.x && a.y == b.y;
}

//
// v2_t
//

#define DECLARE_VECTOR2_OP(name, op) \
fn_local v2_t v2_##name   (v2_t    l, v2_t    r) { return (v2_t){ l.x op r.x, l.y op r.y }; } \
fn_local v2_t v2_s##name  (float   l, v2_t    r) { return (v2_t){ l   op r.x, l   op r.y }; } \
fn_local v2_t v2_##name##s(v2_t    l, float   r) { return (v2_t){ l.x op r  , l.y op r   }; }

DECLARE_VECTOR2_OP(add, +)
DECLARE_VECTOR2_OP(sub, -)
DECLARE_VECTOR2_OP(mul, *)
DECLARE_VECTOR2_OP(div, /)

fn_local v2_t make_v2(float x, float y)
{
    return (v2_t){x,y};
}

fn_local v2_t v2_add3(v2_t a, v2_t b, v2_t c)
{
    v2_t result = {
        a.x + b.x + c.x,
        a.y + b.y + c.y,
    };
    return result;
}

fn_local float v2_dot(v2_t l, v2_t r)
{
    return l.x*r.x + l.y*r.y;
}

fn_local float v2_lensq(v2_t x)
{
    return x.x*x.x + x.y*x.y;
}

fn_local float v2_len(v2_t x)
{
    return sqrt_ss(x.x*x.x + x.y*x.y);
}

fn_local v2_t v2_normalize(v2_t x)
{
    float rcp_len = 1.0f / sqrt_ss(x.x*x.x + x.y*x.y);
    return (v2_t){ rcp_len*x.x, rcp_len*x.y };
}

fn_local v2_t v2_normalize_or_zero(v2_t x)
{
    float len = sqrt_ss(x.x*x.x + x.y*x.y);

    float rcp_len = 0.0f;
    if (len > 0.001f)
        rcp_len = 1.0f / len;

    return (v2_t){ rcp_len*x.x, rcp_len*x.y };
}

fn_local v2_t v2_negate(v2_t x)
{
    return (v2_t){ -x.x, -x.y };
}

fn_local v2_t v2_lerp(v2_t l, v2_t r, v2_t t)
{
    v2_t inv_t = { 1.0f - t.x, 1.0f - t.y };
    return (v2_t){ l.x*inv_t.x + r.x*t.x, l.y*inv_t.y + r.y*t.y };
}

fn_local v2_t v2_lerps(v2_t l, v2_t r, float t)
{
    float inv_t = 1.0f - t;
    return (v2_t){ l.x*inv_t + r.x*t, l.y*inv_t + r.y*t };
}

fn_local v2_t v2_min(v2_t l, v2_t r)
{
    return (v2_t){
        l.x < r.x ? l.x : r.x,
        l.y < r.y ? l.y : r.y,
    };
}

fn_local v2_t v2_smin(float l, v2_t r)
{
    return (v2_t){
        l   < r.x ? l   : r.x,
        l   < r.y ? l   : r.y,
    };
}

fn_local v2_t v2_mins(v2_t l, float r)
{
    return (v2_t){
        l.x < r   ? l.x : r  ,
        l.y < r   ? l.y : r  ,
    };
}

fn_local v2_t v2_max(v2_t l, v2_t r)
{
    return (v2_t){
        l.x > r.x ? l.x : r.x,
        l.y > r.y ? l.y : r.y,
    };
}

fn_local v2_t v2_smax(float l, v2_t r)
{
    return (v2_t){
        l   > r.x ? l   : r.x,
        l   > r.y ? l   : r.y,
    };
}

fn_local v2_t v2_maxs(v2_t l, float r)
{
    return (v2_t){
        l.x > r   ? l.x : r  ,
        l.y > r   ? l.y : r  ,
    };
}

fn_local float v2_minval(v2_t v)
{
    float m;
    m = v.x;
    m = v.y < m ? v.y : m;
    return m;
}

fn_local float v2_maxval(v2_t v)
{
    float m;
    m = v.x;
    m = v.y > m ? v.y : m;
    return m;
}

// v3_t

#define DECLARE_VECTOR3_OP(name, op) \
fn_local v3_t v3_##name   (v3_t    l, v3_t    r) { return (v3_t){ l.x op r.x, l.y op r.y, l.z op r.z }; } \
fn_local v3_t v3_s##name  (float   l, v3_t    r) { return (v3_t){ l   op r.x, l   op r.y, l   op r.z }; } \
fn_local v3_t v3_##name##s(v3_t    l, float   r) { return (v3_t){ l.x op r  , l.y op r  , l.z op r   }; }

DECLARE_VECTOR3_OP(add, +)
DECLARE_VECTOR3_OP(sub, -)
DECLARE_VECTOR3_OP(mul, *)
DECLARE_VECTOR3_OP(div, /)

fn_local v3_t v3_from_scalar(float x)
{
    return (v3_t){x, x, x};
}

fn_local v3_t v3_from_v2(v2_t xy, float z)
{
	return (v3_t){
		.x = xy.x,
		.y = xy.y,
		.z = z,
	};
}

fn_local v3_t make_v3(float x, float y, float z)
{
    return (v3_t){x,y,z};
}

fn_local v3_t v3_add3(v3_t a, v3_t b, v3_t c)
{
    v3_t result = {
        a.x + b.x + c.x,
        a.y + b.y + c.y,
        a.z + b.z + c.z,
    };
    return result;
}

fn_local v3_t v3_sub3(v3_t a, v3_t b, v3_t c)
{
    v3_t result = {
        a.x - b.x - c.x,
        a.y - b.y - c.y,
        a.z - b.z - c.z,
    };
    return result;
}

fn_local float v3_dot(v3_t l, v3_t r)
{
    return l.x*r.x + l.y*r.y + l.z*r.z;
}

fn_local float v3_lensq(v3_t x)
{
    return x.x*x.x + x.y*x.y + x.z*x.z;
}

fn_local float v3_len(v3_t x)
{
    return sqrt_ss(x.x*x.x + x.y*x.y + x.z*x.z);
}

fn_local v3_t v3_normalize(v3_t x)
{
    float rcp_len = 1.0f / sqrt_ss(x.x*x.x + x.y*x.y + x.z*x.z);
    return (v3_t){ rcp_len*x.x, rcp_len*x.y, rcp_len*x.z };
}

fn_local v3_t v3_normalize_or_zero(v3_t x)
{
    float len = sqrt_ss(x.x*x.x + x.y*x.y + x.z*x.z);

    float rcp_len = 0.0f;
    if (len > 0.001f)
        rcp_len = 1.0f / len;

    return (v3_t){ rcp_len*x.x, rcp_len*x.y, rcp_len*x.z };
}

fn_local v3_t v3_negate(v3_t x)
{
    return (v3_t){ -x.x, -x.y, -x.z };
}

fn_local v3_t v3_lerp(v3_t l, v3_t r, v3_t t)
{
    v3_t inv_t = { 1.0f - t.x, 1.0f - t.y, 1.0f - t.z };
    return (v3_t){ l.x*inv_t.x + r.x*t.x, l.y*inv_t.y + r.y*t.y, l.z*inv_t.z + r.z*t.z };
}

fn_local v3_t v3_lerps(v3_t l, v3_t r, float t)
{
    float inv_t = 1.0f - t;
    return (v3_t){ l.x*inv_t + r.x*t, l.y*inv_t + r.y*t, l.z*inv_t + r.z*t };
}

fn_local v3_t v3_min(v3_t l, v3_t r)
{
    return (v3_t){
        l.x < r.x ? l.x : r.x,
        l.y < r.y ? l.y : r.y,
        l.z < r.z ? l.z : r.z,
    };
}

fn_local v3_t v3_smin(float l, v3_t r)
{
    return (v3_t){
        l   < r.x ? l   : r.x,
        l   < r.y ? l   : r.y,
        l   < r.z ? l   : r.z,
    };
}

fn_local v3_t v3_mins(v3_t l, float r)
{
    return (v3_t){
        l.x < r   ? l.x : r  ,
        l.y < r   ? l.y : r  ,
        l.z < r   ? l.z : r  ,
    };
}

fn_local float v3_max3(v3_t v)
{
    float result = (v.x > v.y ? v.x : v.y);
    result = (v.z > result ? v.z : result);
    return result;
}

fn_local v3_t v3_max(v3_t l, v3_t r)
{
    return (v3_t){
        l.x > r.x ? l.x : r.x,
        l.y > r.y ? l.y : r.y,
        l.z > r.z ? l.z : r.z,
    };
}

fn_local v3_t v3_smax(float l, v3_t r)
{
    return (v3_t){
        l   > r.x ? l   : r.x,
        l   > r.y ? l   : r.y,
        l   > r.z ? l   : r.z,
    };
}

fn_local v3_t v3_maxs(v3_t l, float r)
{
    return (v3_t){
        l.x > r   ? l.x : r  ,
        l.y > r   ? l.y : r  ,
        l.z > r   ? l.z : r  ,
    };
}

fn_local float v3_minval(v3_t v)
{
    float m;
    m = v.x;
    m = v.y < m ? v.y : m;
    m = v.z < m ? v.z : m;
    return m;
}

fn_local float v3_maxval(v3_t v)
{
    float m;
    m = v.x;
    m = v.y > m ? v.y : m;
    m = v.z > m ? v.z : m;
    return m;
}

fn_local v3_t cross(v3_t a, v3_t b)
{
    v3_t result;
    result.x = a.y*b.z - a.z*b.y;
    result.y = a.z*b.x - a.x*b.z;
    result.z = a.x*b.y - a.y*b.x;
    return result;
}

static bool v3_equal(v3_t a, v3_t b, float margin)
{
	return (flt_equal(a.x, b.x, margin) &&
			flt_equal(a.y, b.y, margin) &&
			flt_equal(a.z, b.z, margin));
}

static bool v3_equal_exact(v3_t a, v3_t b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

// v4_t

#define DECLARE_VECTOR4_OP(name, op) \
fn_local v4_t v4_##name   (v4_t    l, v4_t    r) { return (v4_t){ l.x op r.x, l.y op r.y, l.z op r.z, l.w op r.w }; } \
fn_local v4_t v4_s##name  (float   l, v4_t    r) { return (v4_t){ l   op r.x, l   op r.y, l   op r.z, l   op r.w }; } \
fn_local v4_t v4_##name##s(v4_t    l, float   r) { return (v4_t){ l.x op r  , l.y op r  , l.z op r  , l.w op r   }; }

DECLARE_VECTOR4_OP(add, +)
DECLARE_VECTOR4_OP(sub, -)
DECLARE_VECTOR4_OP(mul, *)
DECLARE_VECTOR4_OP(div, /)

fn_local v4_t make_v4(float x, float y, float z, float w)
{
    return (v4_t){x,y,z,w};
}

fn_local v4_t v4_from_scalar(float s)
{
	return (v4_t){s, s, s, s};
}

fn_local float v4_dot(v4_t l, v4_t r)
{
    return l.x*r.x + l.y*r.y + l.z*r.z + l.w*r.w;
}

fn_local float v4_lensq(v4_t x)
{
    return x.x*x.x + x.y*x.y + x.z*x.z + x.w*x.w;
}

fn_local float v4_len(v4_t x)
{
    return sqrt_ss(x.x*x.x + x.y*x.y + x.z*x.z + x.w*x.w);
}

fn_local v4_t v4_normalize(v4_t x)
{
    float rcp_len = 1.0f / sqrt_ss(x.x*x.x + x.y*x.y + x.z*x.z + x.w*x.w);
    return (v4_t){ rcp_len*x.x, rcp_len*x.y, rcp_len*x.z, rcp_len*x.w };
}

fn_local v4_t v4_normalize_or_zero(v4_t x)
{
    float len = sqrt_ss(x.x*x.x + x.y*x.y + x.z*x.z + x.w*x.w);

    float rcp_len = 0.0f;
    if (len > 0.001f)
        rcp_len = 1.0f / len;

    return (v4_t){ rcp_len*x.x, rcp_len*x.y, rcp_len*x.z, rcp_len*x.w };
}

fn_local v4_t v4_negate(v4_t x)
{
    return (v4_t){ -x.x, -x.y, -x.z, -x.w };
}

fn_local v4_t v4_lerp(v4_t l, v4_t r, v4_t t)
{
    v4_t inv_t = { 1.0f - t.x, 1.0f - t.y, 1.0f - t.z, 1.0f - t.w };
    return (v4_t){ l.x*inv_t.x + r.x*t.x, l.y*inv_t.y + r.y*t.y, l.z*inv_t.z + r.z*t.z, l.w*inv_t.w + r.w*t.w };
}

fn_local v4_t v4_lerps(v4_t l, v4_t r, float t)
{
    float inv_t = 1.0f - t;
    return (v4_t){ l.x*inv_t + r.x*t, l.y*inv_t + r.y*t, l.z*inv_t + r.z*t, l.w*inv_t + r.w*t };
}

fn_local v4_t v4_min(v4_t l, v4_t r)
{
    return (v4_t){
        l.x < r.x ? l.x : r.x,
        l.y < r.y ? l.y : r.y,
        l.z < r.z ? l.z : r.z,
        l.w < r.w ? l.w : r.w,
    };
}

fn_local v4_t v4_smin(float l, v4_t r)
{
    return (v4_t){
        l   < r.x ? l   : r.x,
        l   < r.y ? l   : r.y,
        l   < r.z ? l   : r.z,
        l   < r.w ? l   : r.w,
    };
}

fn_local v4_t v4_mins(v4_t l, float r)
{
    return (v4_t){
        l.x < r   ? l.x : r  ,
        l.y < r   ? l.y : r  ,
        l.z < r   ? l.z : r  ,
        l.w < r   ? l.w : r  ,
    };
}

fn_local v4_t v4_max(v4_t l, v4_t r)
{
    return (v4_t){
        l.x > r.x ? l.x : r.x,
        l.y > r.y ? l.y : r.y,
        l.z > r.z ? l.z : r.z,
        l.w > r.w ? l.w : r.w,
    };
}

fn_local v4_t v4_smax(float l, v4_t r)
{
    return (v4_t){
        l   > r.x ? l   : r.x,
        l   > r.y ? l   : r.y,
        l   > r.z ? l   : r.z,
        l   > r.w ? l   : r.w,
    };
}

fn_local v4_t v4_maxs(v4_t l, float r)
{
    return (v4_t){
        l.x > r   ? l.x : r  ,
        l.y > r   ? l.y : r  ,
        l.z > r   ? l.z : r  ,
        l.w > r   ? l.w : r  ,
    };
}

fn_local float v4_minval(v4_t v)
{
    float m;
    m = v.x;
    m = v.y < m ? v.y : m;
    m = v.z < m ? v.z : m;
    m = v.w < m ? v.w : m;
    return m;
}

fn_local float v4_maxval(v4_t v)
{
    float m;
    m = v.x;
    m = v.y > m ? v.y : m;
    m = v.z > m ? v.z : m;
    m = v.w > m ? v.w : m;
    return m;
}

fn_local v4_t v4_add3(v4_t a, v4_t b, v4_t c)
{
	v4_t result = {
		a.x + b.x + c.x,
		a.y + b.y + c.y,
		a.z + b.z + c.z,
		a.w + b.w + c.w,
	};
	return result;
}

fn_local v4_t v4_add4(v4_t a, v4_t b, v4_t c, v4_t d)
{
	v4_t result = {
		a.x + b.x + c.x + d.x,
		a.y + b.y + c.y + d.y,
		a.z + b.z + c.z + d.z,
		a.w + b.w + c.w + d.w,
	};
	return result;
}

//
// m2x2_t
//

fn_local float m2x2_determinant(m2x2_t mat)
{
    float result = mat.e[0][0]*mat.e[1][1] - mat.e[1][0]*mat.e[0][1];
    return result;
}

//
// m3x3_t
//

fn_local m3x3_t m3x3_mul(m3x3_t b, m3x3_t a)
{
    m3x3_t result;
    result.e[0][0] = a.e[0][0]*b.e[0][0] + a.e[0][1]*b.e[1][0] + a.e[0][2]*b.e[2][0];
    result.e[0][1] = a.e[0][0]*b.e[0][1] + a.e[0][1]*b.e[1][1] + a.e[0][2]*b.e[2][1];
    result.e[0][2] = a.e[0][0]*b.e[0][2] + a.e[0][1]*b.e[1][2] + a.e[0][2]*b.e[2][2];
    result.e[1][0] = a.e[1][0]*b.e[0][0] + a.e[1][1]*b.e[1][0] + a.e[1][2]*b.e[2][0];
    result.e[1][1] = a.e[1][0]*b.e[0][1] + a.e[1][1]*b.e[1][1] + a.e[1][2]*b.e[2][1];
    result.e[1][2] = a.e[1][0]*b.e[0][2] + a.e[1][1]*b.e[1][2] + a.e[1][2]*b.e[2][2];
    result.e[2][0] = a.e[2][0]*b.e[0][0] + a.e[2][1]*b.e[1][0] + a.e[2][2]*b.e[2][0];
    result.e[2][1] = a.e[2][0]*b.e[0][1] + a.e[2][1]*b.e[1][1] + a.e[2][2]*b.e[2][1];
    result.e[2][2] = a.e[2][0]*b.e[0][2] + a.e[2][1]*b.e[1][2] + a.e[2][2]*b.e[2][2];
    return result;
}

fn_local v3_t m3x3_mulv(m3x3_t a, v3_t b)
{
    v3_t result;
    result.x = a.e[0][0]*b.x + a.e[1][0]*b.y + a.e[2][0]*b.z;
    result.y = a.e[0][1]*b.x + a.e[1][1]*b.y + a.e[2][1]*b.z;
    result.z = a.e[0][2]*b.x + a.e[1][2]*b.y + a.e[2][2]*b.z;
    return result;
}

fn_local void m3x3_apply_scale(m3x3_t *mat, float scale)
{
    mat->e[0][0] *= scale;
    mat->e[0][1] *= scale;
    mat->e[0][2] *= scale;

    mat->e[1][0] *= scale;
    mat->e[1][1] *= scale;
    mat->e[1][2] *= scale;

    mat->e[2][0] *= scale;
    mat->e[2][1] *= scale;
    mat->e[2][2] *= scale;
}

fn_local float m3x3_determinant(m3x3_t mat)
{
    float i = mat.e[0][0]*(mat.e[1][1]*mat.e[2][2] - mat.e[1][1]);
    float j = mat.e[1][0]*(mat.e[0][1]*mat.e[2][2] - mat.e[0][1]);
    float k = mat.e[2][0]*(mat.e[0][1]*mat.e[1][2] - mat.e[0][1]);
    return (i - j + k);
}

fn_local float m3x3_trace(m3x3_t mat)
{
    float xx = mat.e[0][0]*mat.e[0][0];
    float yy = mat.e[1][1]*mat.e[1][1];
    float zz = mat.e[2][2]*mat.e[2][2];
    return (xx + yy + zz);
}

fn_local m2x2_t m3x3_minor(m3x3_t mat, size_t i, size_t j)
{
    m2x2_t minor = M2X2_IDENTITY_STATIC;

    size_t yy = 0;
    for (size_t y = 0; y < 3; y++)
    {
        if (y == j)
        {
            continue;
        }

        size_t xx = 0;
        for (size_t x = 0; x < 3; x++)
        {
            if (x == i)
            {
                continue;
            }

            minor.e[yy][xx] = mat.e[y][x];
            xx++;
        }

        yy++;
    }

    return minor;
}

fn_local float m3x3_cofactor(m3x3_t mat, size_t i, size_t j)
{
    m2x2_t minor = m3x3_minor(mat, i, j);
    float c = (float)(powf(-1.0f, i + 1.0f + j + 1.0f))*m2x2_determinant(minor);
    return c;
}

fn_local m3x3_t m3x3_inverse(m3x3_t mat)
{
    float det = m3x3_determinant(mat);
    float rcp_det = 1.0f / det;

    m3x3_t result;
    for (size_t i = 0; i < 3; i++)
    {
        for (size_t j = 0; j < 3; j++)
        {
            result.e[i][j] = rcp_det*m3x3_cofactor(mat, i, j);
        }
    }

    return result;
}

fn_local m3x3_t m3x3_transpose(m3x3_t mat)
{
    m3x3_t result;
    for (size_t i = 0; i < 3; i++)
    {
        for (size_t j = 0; j < 3; j++)
        {
            result.e[j][i] = mat.e[i][j];
        }
    }
    return result;
}

//
// m4x4
//

fn_local m4x4_t m4x4_mul(m4x4_t b, m4x4_t a)
{
    m4x4_t result;
    result.e[0][0] = a.e[0][0]*b.e[0][0] + a.e[0][1]*b.e[1][0] + a.e[0][2]*b.e[2][0] + a.e[0][3]*b.e[3][0];
    result.e[0][1] = a.e[0][0]*b.e[0][1] + a.e[0][1]*b.e[1][1] + a.e[0][2]*b.e[2][1] + a.e[0][3]*b.e[3][1];
    result.e[0][2] = a.e[0][0]*b.e[0][2] + a.e[0][1]*b.e[1][2] + a.e[0][2]*b.e[2][2] + a.e[0][3]*b.e[3][2];
    result.e[0][3] = a.e[0][0]*b.e[0][3] + a.e[0][1]*b.e[1][3] + a.e[0][2]*b.e[2][3] + a.e[0][3]*b.e[3][3];
    result.e[1][0] = a.e[1][0]*b.e[0][0] + a.e[1][1]*b.e[1][0] + a.e[1][2]*b.e[2][0] + a.e[1][3]*b.e[3][0];
    result.e[1][1] = a.e[1][0]*b.e[0][1] + a.e[1][1]*b.e[1][1] + a.e[1][2]*b.e[2][1] + a.e[1][3]*b.e[3][1];
    result.e[1][2] = a.e[1][0]*b.e[0][2] + a.e[1][1]*b.e[1][2] + a.e[1][2]*b.e[2][2] + a.e[1][3]*b.e[3][2];
    result.e[1][3] = a.e[1][0]*b.e[0][3] + a.e[1][1]*b.e[1][3] + a.e[1][2]*b.e[2][3] + a.e[1][3]*b.e[3][3];
    result.e[2][0] = a.e[2][0]*b.e[0][0] + a.e[2][1]*b.e[1][0] + a.e[2][2]*b.e[2][0] + a.e[2][3]*b.e[3][0];
    result.e[2][1] = a.e[2][0]*b.e[0][1] + a.e[2][1]*b.e[1][1] + a.e[2][2]*b.e[2][1] + a.e[2][3]*b.e[3][1];
    result.e[2][2] = a.e[2][0]*b.e[0][2] + a.e[2][1]*b.e[1][2] + a.e[2][2]*b.e[2][2] + a.e[2][3]*b.e[3][2];
    result.e[2][3] = a.e[2][0]*b.e[0][3] + a.e[2][1]*b.e[1][3] + a.e[2][2]*b.e[2][3] + a.e[2][3]*b.e[3][3];
    result.e[3][0] = a.e[3][0]*b.e[0][0] + a.e[3][1]*b.e[1][0] + a.e[3][2]*b.e[2][0] + a.e[3][3]*b.e[3][0];
    result.e[3][1] = a.e[3][0]*b.e[0][1] + a.e[3][1]*b.e[1][1] + a.e[3][2]*b.e[2][1] + a.e[3][3]*b.e[3][1];
    result.e[3][2] = a.e[3][0]*b.e[0][2] + a.e[3][1]*b.e[1][2] + a.e[3][2]*b.e[2][2] + a.e[3][3]*b.e[3][2];
    result.e[3][3] = a.e[3][0]*b.e[0][3] + a.e[3][1]*b.e[1][3] + a.e[3][2]*b.e[2][3] + a.e[3][3]*b.e[3][3];
    return result;
}

fn_local v4_t m4x4_mulv(m4x4_t a, v4_t b)
{
    v4_t result;
    result.x = a.e[0][0]*b.x + a.e[1][0]*b.y + a.e[2][0]*b.z + a.e[3][0]*b.w;
    result.y = a.e[0][1]*b.x + a.e[1][1]*b.y + a.e[2][1]*b.z + a.e[3][1]*b.w;
    result.z = a.e[0][2]*b.x + a.e[1][2]*b.y + a.e[2][2]*b.z + a.e[3][2]*b.w;
    result.w = a.e[0][3]*b.x + a.e[1][3]*b.y + a.e[2][3]*b.z + a.e[3][3]*b.w;
    return result;
}

fn_local m4x4_t m4x4_from_m3x3(m3x3_t mat)
{
    m4x4_t result;

    result.e[0][0] = mat.e[0][0];
    result.e[0][1] = mat.e[0][1];
    result.e[0][2] = mat.e[0][2];

    result.e[1][0] = mat.e[1][0];
    result.e[1][1] = mat.e[1][1];
    result.e[1][2] = mat.e[1][2];

    result.e[2][0] = mat.e[2][0];
    result.e[2][1] = mat.e[2][1];
    result.e[2][2] = mat.e[2][2];

    result.e[3][0] = 0.0f;
    result.e[3][1] = 0.0f;
    result.e[3][2] = 0.0f;

    result.e[0][3] = 0.0f;
    result.e[1][3] = 0.0f;
    result.e[2][3] = 0.0f;
    result.e[3][3] = 1.0f;

    return result;
}

// generic matrix stuff

// m: augmented matrix
// x: result values
// returns: whether system could be solved
bool solve_system_of_equations(mat_t *m, float *x);

// funky macros

#define GENERIC_VECTOR_OP(name, v) \
    _Generic(v,                    \
        v2_t: v2_##name,           \
        v3_t: v3_##name,           \
        v4_t: v4_##name            \
    )(v)

#define vlensq(v)            GENERIC_VECTOR_OP(lensq, v)
#define vlen(v)              GENERIC_VECTOR_OP(len, v)
#define vlen_sq(v)           GENERIC_VECTOR_OP(lensq, v)
#define vminval(v)           GENERIC_VECTOR_OP(minval, v)
#define vmaxval(v)           GENERIC_VECTOR_OP(maxval, v)
#define normalize(v)         GENERIC_VECTOR_OP(normalize, v)
#define normalize_or_zero(v) GENERIC_VECTOR_OP(normalize_or_zero, v)
#define negate(v)            GENERIC_VECTOR_OP(negate, v)

#define GENERIC_VECTOR_BINARY_OP(name, l, r)                                                                                                  \
    _Generic((l),                                                                                                                             \
        float  : _Generic((r), float: flt_##name,   v2_t: v2_s##name, v3_t: v3_s##name, v4_t: v4_s##name, m3x3_t: NULL,     m4x4_t: NULL),    \
        v2_t   : _Generic((r), float: v2_##name##s, v2_t: v2_##name,  v3_t: NULL,       v4_t: NULL      , m3x3_t: NULL,     m4x4_t: NULL),    \
        v3_t   : _Generic((r), float: v3_##name##s, v2_t: NULL,       v3_t: v3_##name,  v4_t: NULL      , m3x3_t: NULL,     m4x4_t: NULL),    \
        v4_t   : _Generic((r), float: v4_##name##s, v2_t: NULL,       v3_t: NULL,       v4_t: v4_##name , m3x3_t: NULL,     m4x4_t: NULL),    \
        m3x3_t : _Generic((r), float: NULL,         v2_t: NULL,       v3_t: m3x3_mulv,  v4_t: NULL      , m3x3_t: m3x3_mul, m4x4_t: NULL),    \
        m4x4_t : _Generic((r), float: NULL,         v2_t: NULL,       v3_t: NULL,       v4_t: m4x4_mulv , m3x3_t: NULL,     m4x4_t: m4x4_mul) \
    )(l, r)

// please stdlib.h don't do it windows.h don't do it
#undef min
#undef max

#define add(l, r) GENERIC_VECTOR_BINARY_OP(add, l, r)
#define sub(l, r) GENERIC_VECTOR_BINARY_OP(sub, l, r)
#define mul(l, r) GENERIC_VECTOR_BINARY_OP(mul, l, r)
#define div(l, r) GENERIC_VECTOR_BINARY_OP(div, l, r)
#define min(l, r) GENERIC_VECTOR_BINARY_OP(min, l, r)
#define max(l, r) GENERIC_VECTOR_BINARY_OP(max, l, r)
#define saturate(x) (max(0.0f, min(1.0f, x)))

#define dot(l, r) _Generic(l, v2_t: v2_dot, v3_t: v3_dot, v4_t: v4_dot)(l, r)

fn_local void basis_vectors(v3_t d, v3_t up, v3_t *x, v3_t *y, v3_t *z)
{
    *z = negate(d);
    *x = normalize(cross(up, *z));
    *y = normalize(cross(*z, *x));
}

// quat

fn_local quat_t make_quat(v3_t imag, float real)
{
    return (quat_t){ imag.x, imag.y, imag.z, real };
}

fn_local quat_t quat_from_axis_angle(v3_t axis, float angle)
{
    axis = normalize_or_zero(axis);

    float s, c;
    sincos_ss(0.5f*angle, &s, &c);

    quat_t result = make_quat(mul(s, axis), c);
    return result;
}

fn_local quat_t quat_mul(quat_t a, quat_t b)
{
    quat_t result;
    result.x = a.e[3]*b.e[0] + a.e[0]*b.e[3] + a.e[1]*b.e[2] - a.e[2]*b.e[1];
    result.y = a.e[3]*b.e[1] - a.e[0]*b.e[2] + a.e[1]*b.e[3] + a.e[2]*b.e[0];
    result.z = a.e[3]*b.e[2] + a.e[0]*b.e[1] - a.e[1]*b.e[0] + a.e[2]*b.e[3];
    result.w = a.e[3]*b.e[3] - a.e[0]*b.e[0] - a.e[1]*b.e[1] - a.e[2]*b.e[2];
    return result;
}

fn_local quat_t quat_conjugate(quat_t q)
{
    return (quat_t){ -q.x, -q.y, -q.z, q.w };
}

fn_local v3_t quat_imag(quat_t q)
{
    return (v3_t){ q.x, q.y, q.z };
}

fn_local float quat_real(quat_t q)
{
    return q.w;
}

fn_local v3_t quat_rotatev(quat_t q, v3_t v)
{
    quat_t p = make_quat(v, 0.0f);

    quat_t r; 
    r = quat_mul(q, p);
    r = quat_mul(r, quat_conjugate(q));

    return quat_imag(r);
}

fn_local m3x3_t m3x3_from_quat(quat_t q)
{
    m3x3_t result;

    float qx   = 2.0f*q.x*q.x;
    float qy   = 2.0f*q.y*q.y;
    float qz   = 2.0f*q.z*q.z;
    float qxqy = 2.0f*q.x*q.y;
    float qxqz = 2.0f*q.x*q.z;
    float qxqw = 2.0f*q.x*q.w;
    float qyqz = 2.0f*q.y*q.z;
    float qyqw = 2.0f*q.y*q.w;
    float qzqw = 2.0f*q.z*q.w;

    result.e[0][0] = 1.0f - qy - qz;
    result.e[1][1] = 1.0f - qx - qz;
    result.e[2][2] = 1.0f - qx - qy;

    result.e[1][0] = qxqy + qzqw;
    result.e[2][0] = qxqz - qyqw;

    result.e[0][1] = qxqy - qzqw;
    result.e[2][1] = qyqz + qxqw;

    result.e[0][2] = qxqz + qyqw;
    result.e[1][2] = qyqz - qxqw;

    return result;
}

fn_local m4x4_t m4x4_from_quat(quat_t q)
{
    m4x4_t result = m4x4_from_m3x3(m3x3_from_quat(q));
    return result;
}

//
// m4x4_t
//

fn_local m4x4_t transpose(m4x4_t mat)
{
    m4x4_t result;
    for (size_t i = 0; i < 4; i++)
    {
        for (size_t j = 0; j < 4; j++)
        {
            result.e[j][i] = mat.e[i][j];
        }
    }
    return result;
}

fn_local m4x4_t translate(m4x4_t mat, v3_t p)
{
    mat.col[3] = add(mat.col[3], mul(mat.col[0], p.x));
    mat.col[3] = add(mat.col[3], mul(mat.col[1], p.y));
    mat.col[3] = add(mat.col[3], mul(mat.col[2], p.z));
    return mat;
}

fn_local m4x4_t m4x4_from_translation(v3_t p)
{
    m4x4_t mat = M4X4_IDENTITY_STATIC;
    mat.col[3].x = p.x;
    mat.col[3].y = p.y;
    mat.col[3].z = p.z;
    return mat;
}

fn_local m4x4_t m4x4_from_scale(v3_t scale)
{
    m4x4_t mat = M4X4_IDENTITY_STATIC;
    mat.e[0][0] = scale.x;
    mat.e[1][1] = scale.y;
    mat.e[2][2] = scale.z;
    return mat;
}

fn_local m4x4_t m4x4_from_sscale(float scale)
{
    m4x4_t mat = M4X4_IDENTITY_STATIC;
    mat.e[0][0] = scale;
    mat.e[1][1] = scale;
    mat.e[2][2] = scale;
    return mat;
}

fn_local m4x4_t make_view_matrix(v3_t p, v3_t d, v3_t up)
{
    v3_t x, y, z;
    basis_vectors(d, up, &x, &y, &z);

    m4x4_t mat = M4X4_IDENTITY;

    mat.e[0][0] = x.x;
    mat.e[1][0] = x.y;
    mat.e[2][0] = x.z;

    mat.e[0][1] = y.x;
    mat.e[1][1] = y.y;
    mat.e[2][1] = y.z;

    mat.e[0][2] = z.x;
    mat.e[1][2] = z.y;
    mat.e[2][2] = z.z;

    mat = translate(mat, negate(p));
    return mat;
}

fn_local m4x4_t make_orthographic_matrix(float w, float h, float d)
{
    m4x4_t result = make_m4x4(
        2.0f / w, 0.0f    , 0.0f    , 0.0f,
        0.0f    , 2.0f / h, 0.0f    , 0.0f,
        0.0f    , 0.0f    , 1.0f / d, 0.0f,
        0.0f    , 0.0f    , 0.0f    , 1.0f
    );

    return result;
}

// infinite far plane, inverse z
fn_local m4x4_t make_perspective_matrix(float vfov, float w_over_h, float near_plane)
{
    float g = 1.0f / tan_ss(to_radians(0.5f*vfov));
    float s = w_over_h;
    float n = near_plane;

    float e = 0.000001f;
    m4x4_t result = make_m4x4(
        g/s, 0, 0, 0,
        0,   g, 0, 0,
        0,   0, e, n*(1.0f-e),
        0,   0,-1, 0
    );

    return result;
}

//
// rect2i
//

fn_local bool rect2i_contains_inclusive(rect2i_t rect, v2i_t p)
{
    return (p.x >= rect.min.x && p.x <= rect.max.x &&
            p.y >= rect.min.y && p.y <= rect.max.y);
}

fn_local bool rect2i_contains_exclusive(rect2i_t rect, v2i_t p)
{
    return (p.x >= rect.min.x && p.x < rect.max.x &&
            p.y >= rect.min.y && p.y < rect.max.y);
}

fn_local v2i_t rect2i_get_dim(rect2i_t rect)
{
    v2i_t result = {
        rect.max.x - rect.min.x,
        rect.max.y - rect.min.y,
    };
    return result;
}

//
// rect2
//

fn_local rect2_t rect2_min_max(v2_t min, v2_t max)
{
	rect2_t result = {
		.min = min,
		.max = max,
	};
	return result;
}

fn_local rect2_t rect2_from_min_dim(v2_t min, v2_t dim)
{
    rect2_t result = {
        .min = min,
        .max = add(min, dim),
    };
    return result;
}

fn_local rect2_t rect2_center_radius(v2_t center, v2_t radius)
{
    rect2_t result = {
        .min = sub(center, radius),
        .max = add(center, radius),
    };
    return result;
}

fn_local rect2_t rect2_center_dim(v2_t center, v2_t dim)
{
    rect2_t result = {
        .min = sub(center, mul(0.5f, dim)),
        .max = add(center, mul(0.5f, dim)),
    };
    return result;
}

fn_local rect2_t rect2_add_radius(rect2_t rect, v2_t radius)
{
    rect2_t result = {
        .min = sub(rect.min, radius),
        .max = add(rect.max, radius),
    };
    return result;
}

fn_local rect2_t rect2_sub_radius(rect2_t rect, v2_t radius)
{
    rect2_t result = {
        .min = add(rect.min, radius),
        .max = sub(rect.max, radius),
    };
    return result;
}

fn_local bool rect2_contains_point(rect2_t rect, v2_t point)
{
    return (point.x >= rect.min.x && point.x < rect.max.x &&
            point.y >= rect.min.y && point.y < rect.max.y);
}

fn_local rect2_t rect2_union(rect2_t a, rect2_t b)
{
    rect2_t result = {
        .min = min(a.min, b.min),
        .max = max(a.max, b.max),
    };
    return result;
}

fn_local rect2_t rect2_intersect(rect2_t a, rect2_t b)
{
    rect2_t result = {
        .min = max(a.min, b.min),
        .max = min(a.max, b.max),
    };
    return result;
}

fn_local rect2_t rect2_uninvert(rect2_t rect)
{
	rect2_t result = {
		.min = min(rect.min, rect.max),
		.max = max(rect.min, rect.max),
	};
	return result;
}

fn_local rect2_t rect2_inverted_infinity(void)
{
    rect2_t result = {
        .min = {  FLT_MAX,  FLT_MAX },
        .max = { -FLT_MAX, -FLT_MAX },
    };
    return result;
}

fn_local rect2_t rect2_infinity(void)
{
    rect2_t result = {
        .min = { -FLT_MAX, -FLT_MAX },
        .max = {  FLT_MAX,  FLT_MAX },
    };
    return result;
}

fn_local rect2_t rect2_add_offset(rect2_t rect, v2_t offset)
{
	rect2_t result = {
		.min = add(rect.min, offset),
		.max = add(rect.max, offset),
	};
	return result;
}

fn_local v2_t rect2_center(rect2_t rect)
{
	return mul(0.5f, add(rect.min, rect.max));
}

fn_local v2_t rect2_dim(rect2_t rect)
{
    v2_t result = {
        rect.max.x - rect.min.x,
        rect.max.y - rect.min.y,
    };
    return result;
}

fn_local float rect2_width(rect2_t rect)
{
	return rect.max.x - rect.min.x;
}

fn_local float rect2_height(rect2_t rect)
{
	return rect.max.y - rect.min.y;
}

fn_local float rect2_area(rect2_t rect)
{
	float w = rect2_width(rect);
	float h = rect2_height(rect);
	return w*h;
}

fn_local rect2_t rect2_reposition_min(rect2_t rect, v2_t p)
{
    v2_t dim = rect2_dim(rect);
    return rect2_from_min_dim(p, dim);
}

//
// rect-cut
//

fn_local rect2_t rect2_cut_left(rect2_t *rect, float a)
{
	float min_x = rect->x0;
	rect->min.x = rect->x0 + a;
	return (rect2_t){ .min = { min_x, rect->min.y }, .max = { rect->min.x, rect->max.y } };
}

fn_local rect2_t rect2_cut_right(rect2_t *rect, float a)
{
	float max_x = rect->x1;
	rect->max.x = rect->x1 - a;
	return (rect2_t){ .min = { rect->max.x, rect->min.y }, .max = { max_x, rect->max.y } };
}

fn_local rect2_t rect2_cut_top(rect2_t *rect, float a)
{
	float max_y = rect->y1;
	rect->max.y = rect->y1 - a;
	return (rect2_t){ .min = { rect->min.x, rect->max.y }, .max = { rect->max.x, max_y } };
}

fn_local rect2_t rect2_cut_bottom(rect2_t *rect, float a)
{
	float min_y = rect->y0;
	rect->min.y = rect->y0 + a;
	return (rect2_t){ .min = { rect->min.x, min_y }, .max = { rect->max.x, rect->min.y } };
}

fn_local rect2_t rect2_add_left(rect2_t rect, float a)
{
	float min_x = MIN(rect.x0, rect.x0 - a);
	return (rect2_t){ .min = { min_x, rect.min.y }, .max = { rect.min.x, rect.max.y } };
}

fn_local rect2_t rect2_add_right(rect2_t rect, float a)
{
	float max_x = MAX(rect.x0, rect.x0 + a);
	return (rect2_t){ .min = { rect.max.x, rect.min.y }, .max = { max_x, rect.max.y } };
}

fn_local rect2_t rect2_add_top(rect2_t rect, float a)
{
	float max_y = MAX(rect.y1, rect.y1 + a);
	return (rect2_t){ .min = { rect.min.x, rect.max.y }, .max = { rect.max.x, max_y } };
}

fn_local rect2_t rect2_add_bottom(rect2_t rect, float a)
{
	float min_y = MIN(rect.y0, rect.y0 - a);
	return (rect2_t){ .min = { rect.min.x, min_y }, .max = { rect.max.x, rect.min.y } };
}

fn_local rect2_t rect2_extend(rect2_t rect, float a)
{
	rect2_t result = {
		.min = sub(rect.min, a),
		.max = add(rect.max, a),
	};
	return result;
}

fn_local rect2_t rect2_extend_right(rect2_t rect, float a)
{
	rect2_t result = {
		.min = { rect.min.x    , rect.min.y     },
		.max = { rect.max.x + a, rect.max.y     },
	};
	return result;
}

fn_local rect2_t rect2_extend_left(rect2_t rect, float a)
{
	rect2_t result = {
		.min = { rect.min.x - a, rect.min.y     },
		.max = { rect.max.x    , rect.max.y     },
	};
	return result;
}

fn_local rect2_t rect2_extend_up(rect2_t rect, float a)
{
	rect2_t result = {
		.min = { rect.min.x    , rect.min.y     },
		.max = { rect.max.x    , rect.max.y + a },
	};
	return result;
}

fn_local rect2_t rect2_extend_down(rect2_t rect, float a)
{
	rect2_t result = {
		.min = { rect.min.x    , rect.min.y - a },
		.max = { rect.max.x    , rect.max.y     },
	};
	return result;
}

fn_local rect2_t rect2_shrink(rect2_t rect, float a)
{
	return rect2_extend(rect, -a);
}

fn_local rect2_t rect2_extend2(rect2_t rect, float x, float y)
{
	v2_t xy = { x, y };

	rect2_t result = {
		.min = sub(rect.min, xy),
		.max = add(rect.max, xy),
	};
	return result;
}

fn_local rect2_t rect2_pillarbox(rect2_t rect, float a)
{
	return rect2_extend2(rect, -a, 0);
}

fn_local rect2_t rect2_shrink2(rect2_t rect, float x, float y)
{
	return rect2_extend2(rect, -x, -y);
}

typedef enum rect2_cut_side_t
{
	RECT2_CUT_LEFT,
	RECT2_CUT_RIGHT,
	RECT2_CUT_TOP,
	RECT2_CUT_BOTTOM,
	RECT2_CUT_COUNT,
} rect2_cut_side_t;

typedef struct rect2_cut_t
{
	rect2_t         *rect;
	rect2_cut_side_t side;
} rect2_cut_t;

fn_local rect2_t rect2_do_cut(rect2_cut_t cut, float a)
{
	switch (cut.side)
	{
		case RECT2_CUT_LEFT:   return rect2_cut_left  (cut.rect, a);
		case RECT2_CUT_RIGHT:  return rect2_cut_right (cut.rect, a);
		case RECT2_CUT_TOP:    return rect2_cut_top   (cut.rect, a);
		case RECT2_CUT_BOTTOM: return rect2_cut_bottom(cut.rect, a);
		INVALID_DEFAULT_CASE;
	}

	return (rect2_t){ 0 };
}

//
// rect3
//

fn_local rect3_t rect3_inverted_infinity(void)
{
    rect3_t result = {
        .min = { FLT_MAX, FLT_MAX, FLT_MAX },
        .max = {-FLT_MAX,-FLT_MAX,-FLT_MAX },
    };
    return result;
}

fn_local rect3_t rect3_grow_to_contain(rect3_t rect, v3_t p)
{
    rect3_t result = {
        .min = min(rect.min, p),
        .max = max(rect.max, p),
    };
    return result;
}

fn_local rect3_t rect3_union(rect3_t a, rect3_t b)
{
    rect3_t result = {
        .min = min(a.min, b.min),
        .max = max(a.max, b.max),
    };
    return result;
}

fn_local rect3_t rect3_add(rect3_t a, rect3_t b)
{
    rect3_t result = {
        .min = add(a.min, b.min),
        .max = add(a.max, b.max),
    };
    return result;
}

fn_local rect3_t rect3_grow_radius(rect3_t rect, v3_t radius)
{
    rect3_t result = {
        .min = sub(rect.min, radius),
        .max = add(rect.max, radius),
    };
    return result;
}

fn_local v3_t rect3_center(rect3_t rect)
{
    return mul(0.5f, add(rect.min, rect.max));
}

fn_local v3_t rect3_dim(rect3_t rect)
{
    return sub(rect.max, rect.min);
}

fn_local rect3_t rect3_center_radius(v3_t center, v3_t radius)
{
    rect3_t result = {
        .min = sub(center, radius),
        .max = add(center, radius),
    };
    return result;
}

fn_local uint8_t rect3_largest_axis(rect3_t rect)
{
    v3_t dim = {
        rect.max.x - rect.min.x,
        rect.max.y - rect.min.y,
        rect.max.z - rect.min.z,
    };

    uint8_t result = 0;

    float largest = dim.x;
    if (dim.y > largest)
    {
        result = 1;
        largest = dim.y;
    }
    if (dim.z > largest)
    {
        result = 2;
    }

    return result;
}

fn_local void get_tangent_vectors(v3_t n, v3_t *t, v3_t *b)
{
    v3_t up = { 0, 0, 1 };
    *t = cross(up, n);

    if (vlen(*t) <= 0.01f)
    {
        up = (v3_t) { 0, 1, 0 };
        *t = cross(up, n);
    }

    *t = normalize(*t);
    *b = normalize(cross(n, *t));
}

// points go clockwise
fn_local void plane_from_points(v3_t a, v3_t b, v3_t c, plane_t *p)
{
    p->n = normalize(cross(sub(c, a), sub(b, a)));
    p->d = dot(p->n, a);
}

fn_local float triangle_area(v3_t a, v3_t b, v3_t c)
{
	v3_t ab = sub(b, a);
	v3_t ac = sub(c, a);
	v3_t perp = cross(ab, ac);

	float area = 0.5f*vlen(perp);
	return area;
}

fn_local float triangle_area_sq(v3_t a, v3_t b, v3_t c)
{
	v3_t ab = sub(b, a);
	v3_t ac = sub(c, a);
	v3_t perp = cross(ab, ac);

	float area = 0.5f*vlensq(perp);
	return area;
}

fn_local v3_t triangle_normal(v3_t a, v3_t b, v3_t c)
{
	v3_t ab = normalize(sub(b, a));
	v3_t ac = normalize(sub(c, a));

	v3_t normal = normalize(cross(ab, ac));
	return normal;
}

fn_local float tetrahedron_signed_volume(v3_t a, v3_t b, v3_t c, v3_t d)
{
	v3_t ad = sub(d, a);
	v3_t ab = sub(b, a);
	v3_t ac = sub(c, a);

	float volume = (1.0f / 6.0f)*dot(ad, cross(ab, ac));
	return volume;
}

#include "geometric_algebra.h"

fn_local v3_t forward_vector_from_pitch_yaw(float pitch, float yaw)
{
    rotor3_t r_pitch = rotor3_from_plane_angle(PLANE_ZX, DEG_TO_RAD*pitch);
    rotor3_t r_yaw   = rotor3_from_plane_angle(PLANE_YZ, DEG_TO_RAD*yaw  );

    v3_t forward = { 1, 0, 0 };
    forward = rotor3_rotatev(r_pitch, forward);
    forward = rotor3_rotatev(r_yaw,   forward);

    return forward;
}

// Steam hardware survey says SSE2 availability is 100%
// That seems good enough to me.
#define USE_SSE2

// below is Julien Pommier's sse_mathfun.h (http://gruntthepeon.free.fr/ssemath/) with minor alterations:
// copyright/license applies as follows:

/* SIMD (SSE1+MMX or SSE2) implementation of sin, cos, exp and log

   Inspired by Intel Approximate Math library, and based on the
   corresponding algorithms of the cephes math library

   The default is to use the SSE1 version. If you define USE_SSE2 the
   the SSE2 intrinsics will be used in place of the MMX intrinsics. Do
   not expect any significant performance improvement with SSE2.
*/

/* Copyright (C) 2007  Julien Pommier

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  (this is the zlib license)
*/

/* yes I know, the top of this file is quite ugly */

#ifdef _MSC_VER /* visual c++ */
# define ALIGN16_BEG __declspec(align(16))
# define ALIGN16_END 
#else /* gcc or icc */
# define ALIGN16_BEG
# define ALIGN16_END __attribute__((aligned(16)))
#endif

#ifdef USE_SSE2
# include <emmintrin.h>
typedef __m128i v4si; // vector of 4 int (sse2)
#else
typedef __m64 v2si;   // vector of 2 int (mmx)
#endif

/* declare some SSE constants -- why can't I figure a better way to do that? */
#define _PS_CONST(Name, Val)                                            \
  static const ALIGN16_BEG float _ps_##Name[4] ALIGN16_END = { (float)Val, (float)Val, (float)Val, (float)Val }
#define _PI32_CONST(Name, Val)                                            \
  static const ALIGN16_BEG int _pi32_##Name[4] ALIGN16_END = { Val, Val, Val, Val }
#define _PS_CONST_TYPE(Name, Type, Val)                                 \
  static const ALIGN16_BEG Type _ps_##Name[4] ALIGN16_END = { Val, Val, Val, Val }

_PS_CONST(1  , 1.0f);
_PS_CONST(0p5, 0.5f);
/* the smallest non denormalized float number */
_PS_CONST_TYPE(min_norm_pos, int, 0x00800000);
_PS_CONST_TYPE(mant_mask, int, 0x7f800000);
_PS_CONST_TYPE(inv_mant_mask, int, ~0x7f800000);

_PS_CONST_TYPE(sign_mask, int, (int)0x80000000);
_PS_CONST_TYPE(inv_sign_mask, int, ~0x80000000);

_PI32_CONST(1, 1);
_PI32_CONST(inv1, ~1);
_PI32_CONST(2, 2);
_PI32_CONST(4, 4);
_PI32_CONST(0x7f, 0x7f);

_PS_CONST(cephes_SQRTHF, 0.707106781186547524);
_PS_CONST(cephes_log_p0, 7.0376836292E-2);
_PS_CONST(cephes_log_p1, - 1.1514610310E-1);
_PS_CONST(cephes_log_p2, 1.1676998740E-1);
_PS_CONST(cephes_log_p3, - 1.2420140846E-1);
_PS_CONST(cephes_log_p4, + 1.4249322787E-1);
_PS_CONST(cephes_log_p5, - 1.6668057665E-1);
_PS_CONST(cephes_log_p6, + 2.0000714765E-1);
_PS_CONST(cephes_log_p7, - 2.4999993993E-1);
_PS_CONST(cephes_log_p8, + 3.3333331174E-1);
_PS_CONST(cephes_log_q1, -2.12194440e-4);
_PS_CONST(cephes_log_q2, 0.693359375);

#ifndef USE_SSE2
typedef union xmm_mm_union {
  __m128 xmm;
  __m64 mm[2];
} xmm_mm_union;

#define COPY_XMM_TO_MM(xmm_, mm0_, mm1_) {          \
    xmm_mm_union u; u.xmm = xmm_;                   \
    mm0_ = u.mm[0];                                 \
    mm1_ = u.mm[1];                                 \
}

#define COPY_MM_TO_XMM(mm0_, mm1_, xmm_) {                         \
    xmm_mm_union u; u.mm[0]=mm0_; u.mm[1]=mm1_; xmm_ = u.xmm;      \
  }

#endif // USE_SSE2

/* natural logarithm computed for 4 simultaneous float 
   return NaN for x <= 0
*/
v4sf log_ps(v4sf x) {
#ifdef USE_SSE2
  v4si emm0;
#else
  v2si mm0, mm1;
#endif
  v4sf one = *(v4sf*)_ps_1;

  v4sf invalid_mask = _mm_cmple_ps(x, _mm_setzero_ps());

  x = _mm_max_ps(x, *(v4sf*)_ps_min_norm_pos);  /* cut off denormalized stuff */

#ifndef USE_SSE2
  /* part 1: x = frexpf(x, &e); */
  COPY_XMM_TO_MM(x, mm0, mm1);
  mm0 = _mm_srli_pi32(mm0, 23);
  mm1 = _mm_srli_pi32(mm1, 23);
#else
  emm0 = _mm_srli_epi32(_mm_castps_si128(x), 23);
#endif
  /* keep only the fractional part */
  x = _mm_and_ps(x, *(v4sf*)_ps_inv_mant_mask);
  x = _mm_or_ps(x, *(v4sf*)_ps_0p5);

#ifndef USE_SSE2
  /* now e=mm0:mm1 contain the really base-2 exponent */
  mm0 = _mm_sub_pi32(mm0, *(v2si*)_pi32_0x7f);
  mm1 = _mm_sub_pi32(mm1, *(v2si*)_pi32_0x7f);
  v4sf e = _mm_cvtpi32x2_ps(mm0, mm1);
  _mm_empty(); /* bye bye mmx */
#else
  emm0 = _mm_sub_epi32(emm0, *(v4si*)_pi32_0x7f);
  v4sf e = _mm_cvtepi32_ps(emm0);
#endif

  e = _mm_add_ps(e, one);

  /* part2: 
     if( x < SQRTHF ) {
       e -= 1;
       x = x + x - 1.0;
     } else { x = x - 1.0; }
  */
  v4sf mask = _mm_cmplt_ps(x, *(v4sf*)_ps_cephes_SQRTHF);
  v4sf tmp = _mm_and_ps(x, mask);
  x = _mm_sub_ps(x, one);
  e = _mm_sub_ps(e, _mm_and_ps(one, mask));
  x = _mm_add_ps(x, tmp);


  v4sf z = _mm_mul_ps(x,x);

  v4sf y = *(v4sf*)_ps_cephes_log_p0;
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p1);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p2);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p3);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p4);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p5);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p6);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p7);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_log_p8);
  y = _mm_mul_ps(y, x);

  y = _mm_mul_ps(y, z);
  

  tmp = _mm_mul_ps(e, *(v4sf*)_ps_cephes_log_q1);
  y = _mm_add_ps(y, tmp);


  tmp = _mm_mul_ps(z, *(v4sf*)_ps_0p5);
  y = _mm_sub_ps(y, tmp);

  tmp = _mm_mul_ps(e, *(v4sf*)_ps_cephes_log_q2);
  x = _mm_add_ps(x, y);
  x = _mm_add_ps(x, tmp);
  x = _mm_or_ps(x, invalid_mask); // negative arg will be NAN
  return x;
}

_PS_CONST(exp_hi,	88.3762626647949f);
_PS_CONST(exp_lo,	-88.3762626647949f);

_PS_CONST(cephes_LOG2EF, 1.44269504088896341);
_PS_CONST(cephes_exp_C1, 0.693359375);
_PS_CONST(cephes_exp_C2, -2.12194440e-4);

_PS_CONST(cephes_exp_p0, 1.9875691500E-4);
_PS_CONST(cephes_exp_p1, 1.3981999507E-3);
_PS_CONST(cephes_exp_p2, 8.3334519073E-3);
_PS_CONST(cephes_exp_p3, 4.1665795894E-2);
_PS_CONST(cephes_exp_p4, 1.6666665459E-1);
_PS_CONST(cephes_exp_p5, 5.0000001201E-1);

v4sf exp_ps(v4sf x) {
  v4sf tmp = _mm_setzero_ps(), fx;
#ifdef USE_SSE2
  v4si emm0;
#else
  v2si mm0, mm1;
#endif
  v4sf one = *(v4sf*)_ps_1;

  x = _mm_min_ps(x, *(v4sf*)_ps_exp_hi);
  x = _mm_max_ps(x, *(v4sf*)_ps_exp_lo);

  /* express exp(x) as exp(g + n*log(2)) */
  fx = _mm_mul_ps(x, *(v4sf*)_ps_cephes_LOG2EF);
  fx = _mm_add_ps(fx, *(v4sf*)_ps_0p5);

  /* how to perform a floorf with SSE: just below */
#ifndef USE_SSE2
  /* step 1 : cast to int */
  tmp = _mm_movehl_ps(tmp, fx);
  mm0 = _mm_cvttps_pi32(fx);
  mm1 = _mm_cvttps_pi32(tmp);
  /* step 2 : cast back to float */
  tmp = _mm_cvtpi32x2_ps(mm0, mm1);
#else
  emm0 = _mm_cvttps_epi32(fx);
  tmp  = _mm_cvtepi32_ps(emm0);
#endif
  /* if greater, substract 1 */
  v4sf mask = _mm_cmpgt_ps(tmp, fx);    
  mask = _mm_and_ps(mask, one);
  fx = _mm_sub_ps(tmp, mask);

  tmp = _mm_mul_ps(fx, *(v4sf*)_ps_cephes_exp_C1);
  v4sf z = _mm_mul_ps(fx, *(v4sf*)_ps_cephes_exp_C2);
  x = _mm_sub_ps(x, tmp);
  x = _mm_sub_ps(x, z);

  z = _mm_mul_ps(x,x);
  
  v4sf y = *(v4sf*)_ps_cephes_exp_p0;
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_exp_p1);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_exp_p2);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_exp_p3);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_exp_p4);
  y = _mm_mul_ps(y, x);
  y = _mm_add_ps(y, *(v4sf*)_ps_cephes_exp_p5);
  y = _mm_mul_ps(y, z);
  y = _mm_add_ps(y, x);
  y = _mm_add_ps(y, one);

  /* build 2^n */
#ifndef USE_SSE2
  z = _mm_movehl_ps(z, fx);
  mm0 = _mm_cvttps_pi32(fx);
  mm1 = _mm_cvttps_pi32(z);
  mm0 = _mm_add_pi32(mm0, *(v2si*)_pi32_0x7f);
  mm1 = _mm_add_pi32(mm1, *(v2si*)_pi32_0x7f);
  mm0 = _mm_slli_pi32(mm0, 23); 
  mm1 = _mm_slli_pi32(mm1, 23);
  
  v4sf pow2n; 
  COPY_MM_TO_XMM(mm0, mm1, pow2n);
  _mm_empty();
#else
  emm0 = _mm_cvttps_epi32(fx);
  emm0 = _mm_add_epi32(emm0, *(v4si*)_pi32_0x7f);
  emm0 = _mm_slli_epi32(emm0, 23);
  v4sf pow2n = _mm_castsi128_ps(emm0);
#endif
  y = _mm_mul_ps(y, pow2n);
  return y;
}

_PS_CONST(minus_cephes_DP1, -0.78515625);
_PS_CONST(minus_cephes_DP2, -2.4187564849853515625e-4);
_PS_CONST(minus_cephes_DP3, -3.77489497744594108e-8);
_PS_CONST(sincof_p0, -1.9515295891E-4);
_PS_CONST(sincof_p1,  8.3321608736E-3);
_PS_CONST(sincof_p2, -1.6666654611E-1);
_PS_CONST(coscof_p0,  2.443315711809948E-005);
_PS_CONST(coscof_p1, -1.388731625493765E-003);
_PS_CONST(coscof_p2,  4.166664568298827E-002);
_PS_CONST(cephes_FOPI, 1.27323954473516); // 4 / M_PI


/* evaluation of 4 sines at onces, using only SSE1+MMX intrinsics so
   it runs also on old athlons XPs and the pentium III of your grand
   mother.

   The code is the exact rewriting of the cephes sinf function.
   Precision is excellent as long as x < 8192 (I did not bother to
   take into account the special handling they have for greater values
   -- it does not return garbage for arguments over 8192, though, but
   the extra precision is missing).

   Note that it is such that sinf((float)M_PI) = 8.74e-8, which is the
   surprising but correct result.

   Performance is also surprisingly good, 1.33 times faster than the
   macos vsinf SSE2 function, and 1.5 times faster than the
   __vrs4_sinf of amd's ACML (which is only available in 64 bits). Not
   too bad for an SSE1 function (with no special tuning) !
   However the latter libraries probably have a much better handling of NaN,
   Inf, denormalized and other special arguments..

   On my core 1 duo, the execution of this function takes approximately 95 cycles.

   From what I have observed on the experiments with Intel AMath lib, switching to an
   SSE2 version would improve the perf by only 10%.

   Since it is based on SSE intrinsics, it has to be compiled at -O2 to
   deliver full speed.
*/
v4sf sin_ps(v4sf x) { // any x
  v4sf xmm1, xmm2 = _mm_setzero_ps(), xmm3, sign_bit, y;

#ifdef USE_SSE2
  v4si emm0, emm2;
#else
  v2si mm0, mm1, mm2, mm3;
#endif
  sign_bit = x;
  /* take the absolute value */
  x = _mm_and_ps(x, *(v4sf*)_ps_inv_sign_mask);
  /* extract the sign bit (upper one) */
  sign_bit = _mm_and_ps(sign_bit, *(v4sf*)_ps_sign_mask);
  
  /* scale by 4/Pi */
  y = _mm_mul_ps(x, *(v4sf*)_ps_cephes_FOPI);

#ifdef USE_SSE2
  /* store the integer part of y in mm0 */
  emm2 = _mm_cvttps_epi32(y);
  /* j=(j+1) & (~1) (see the cephes sources) */
  emm2 = _mm_add_epi32(emm2, *(v4si*)_pi32_1);
  emm2 = _mm_and_si128(emm2, *(v4si*)_pi32_inv1);
  y = _mm_cvtepi32_ps(emm2);

  /* get the swap sign flag */
  emm0 = _mm_and_si128(emm2, *(v4si*)_pi32_4);
  emm0 = _mm_slli_epi32(emm0, 29);
  /* get the polynom selection mask 
     there is one polynom for 0 <= x <= Pi/4
     and another one for Pi/4<x<=Pi/2

     Both branches will be computed.
  */
  emm2 = _mm_and_si128(emm2, *(v4si*)_pi32_2);
  emm2 = _mm_cmpeq_epi32(emm2, _mm_setzero_si128());
  
  v4sf swap_sign_bit = _mm_castsi128_ps(emm0);
  v4sf poly_mask = _mm_castsi128_ps(emm2);
  sign_bit = _mm_xor_ps(sign_bit, swap_sign_bit);
  
#else
  /* store the integer part of y in mm0:mm1 */
  xmm2 = _mm_movehl_ps(xmm2, y);
  mm2 = _mm_cvttps_pi32(y);
  mm3 = _mm_cvttps_pi32(xmm2);
  /* j=(j+1) & (~1) (see the cephes sources) */
  mm2 = _mm_add_pi32(mm2, *(v2si*)_pi32_1);
  mm3 = _mm_add_pi32(mm3, *(v2si*)_pi32_1);
  mm2 = _mm_and_si64(mm2, *(v2si*)_pi32_inv1);
  mm3 = _mm_and_si64(mm3, *(v2si*)_pi32_inv1);
  y = _mm_cvtpi32x2_ps(mm2, mm3);
  /* get the swap sign flag */
  mm0 = _mm_and_si64(mm2, *(v2si*)_pi32_4);
  mm1 = _mm_and_si64(mm3, *(v2si*)_pi32_4);
  mm0 = _mm_slli_pi32(mm0, 29);
  mm1 = _mm_slli_pi32(mm1, 29);
  /* get the polynom selection mask */
  mm2 = _mm_and_si64(mm2, *(v2si*)_pi32_2);
  mm3 = _mm_and_si64(mm3, *(v2si*)_pi32_2);
  mm2 = _mm_cmpeq_pi32(mm2, _mm_setzero_si64());
  mm3 = _mm_cmpeq_pi32(mm3, _mm_setzero_si64());
  v4sf swap_sign_bit, poly_mask;
  COPY_MM_TO_XMM(mm0, mm1, swap_sign_bit);
  COPY_MM_TO_XMM(mm2, mm3, poly_mask);
  sign_bit = _mm_xor_ps(sign_bit, swap_sign_bit);
  _mm_empty(); /* good-bye mmx */
#endif
  
  /* The magic pass: "Extended precision modular arithmetic" 
     x = ((x - y * DP1) - y * DP2) - y * DP3; */
  xmm1 = *(v4sf*)_ps_minus_cephes_DP1;
  xmm2 = *(v4sf*)_ps_minus_cephes_DP2;
  xmm3 = *(v4sf*)_ps_minus_cephes_DP3;
  xmm1 = _mm_mul_ps(y, xmm1);
  xmm2 = _mm_mul_ps(y, xmm2);
  xmm3 = _mm_mul_ps(y, xmm3);
  x = _mm_add_ps(x, xmm1);
  x = _mm_add_ps(x, xmm2);
  x = _mm_add_ps(x, xmm3);

  /* Evaluate the first polynom  (0 <= x <= Pi/4) */
  y = *(v4sf*)_ps_coscof_p0;
  v4sf z = _mm_mul_ps(x,x);

  y = _mm_mul_ps(y, z);
  y = _mm_add_ps(y, *(v4sf*)_ps_coscof_p1);
  y = _mm_mul_ps(y, z);
  y = _mm_add_ps(y, *(v4sf*)_ps_coscof_p2);
  y = _mm_mul_ps(y, z);
  y = _mm_mul_ps(y, z);
  v4sf tmp = _mm_mul_ps(z, *(v4sf*)_ps_0p5);
  y = _mm_sub_ps(y, tmp);
  y = _mm_add_ps(y, *(v4sf*)_ps_1);
  
  /* Evaluate the second polynom  (Pi/4 <= x <= 0) */

  v4sf y2 = *(v4sf*)_ps_sincof_p0;
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_add_ps(y2, *(v4sf*)_ps_sincof_p1);
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_add_ps(y2, *(v4sf*)_ps_sincof_p2);
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_mul_ps(y2, x);
  y2 = _mm_add_ps(y2, x);

  /* select the correct result from the two polynoms */  
  xmm3 = poly_mask;
  y2 = _mm_and_ps(xmm3, y2); //, xmm3);
  y = _mm_andnot_ps(xmm3, y);
  y = _mm_add_ps(y,y2);
  /* update the sign */
  y = _mm_xor_ps(y, sign_bit);
  return y;
}

/* almost the same as sin_ps */
v4sf cos_ps(v4sf x) { // any x
  v4sf xmm1, xmm2 = _mm_setzero_ps(), xmm3, y;
#ifdef USE_SSE2
  v4si emm0, emm2;
#else
  v2si mm0, mm1, mm2, mm3;
#endif
  /* take the absolute value */
  x = _mm_and_ps(x, *(v4sf*)_ps_inv_sign_mask);
  
  /* scale by 4/Pi */
  y = _mm_mul_ps(x, *(v4sf*)_ps_cephes_FOPI);
  
#ifdef USE_SSE2
  /* store the integer part of y in mm0 */
  emm2 = _mm_cvttps_epi32(y);
  /* j=(j+1) & (~1) (see the cephes sources) */
  emm2 = _mm_add_epi32(emm2, *(v4si*)_pi32_1);
  emm2 = _mm_and_si128(emm2, *(v4si*)_pi32_inv1);
  y = _mm_cvtepi32_ps(emm2);

  emm2 = _mm_sub_epi32(emm2, *(v4si*)_pi32_2);
  
  /* get the swap sign flag */
  emm0 = _mm_andnot_si128(emm2, *(v4si*)_pi32_4);
  emm0 = _mm_slli_epi32(emm0, 29);
  /* get the polynom selection mask */
  emm2 = _mm_and_si128(emm2, *(v4si*)_pi32_2);
  emm2 = _mm_cmpeq_epi32(emm2, _mm_setzero_si128());
  
  v4sf sign_bit = _mm_castsi128_ps(emm0);
  v4sf poly_mask = _mm_castsi128_ps(emm2);
#else
  /* store the integer part of y in mm0:mm1 */
  xmm2 = _mm_movehl_ps(xmm2, y);
  mm2 = _mm_cvttps_pi32(y);
  mm3 = _mm_cvttps_pi32(xmm2);

  /* j=(j+1) & (~1) (see the cephes sources) */
  mm2 = _mm_add_pi32(mm2, *(v2si*)_pi32_1);
  mm3 = _mm_add_pi32(mm3, *(v2si*)_pi32_1);
  mm2 = _mm_and_si64(mm2, *(v2si*)_pi32_inv1);
  mm3 = _mm_and_si64(mm3, *(v2si*)_pi32_inv1);

  y = _mm_cvtpi32x2_ps(mm2, mm3);


  mm2 = _mm_sub_pi32(mm2, *(v2si*)_pi32_2);
  mm3 = _mm_sub_pi32(mm3, *(v2si*)_pi32_2);

  /* get the swap sign flag in mm0:mm1 and the 
     polynom selection mask in mm2:mm3 */

  mm0 = _mm_andnot_si64(mm2, *(v2si*)_pi32_4);
  mm1 = _mm_andnot_si64(mm3, *(v2si*)_pi32_4);
  mm0 = _mm_slli_pi32(mm0, 29);
  mm1 = _mm_slli_pi32(mm1, 29);

  mm2 = _mm_and_si64(mm2, *(v2si*)_pi32_2);
  mm3 = _mm_and_si64(mm3, *(v2si*)_pi32_2);

  mm2 = _mm_cmpeq_pi32(mm2, _mm_setzero_si64());
  mm3 = _mm_cmpeq_pi32(mm3, _mm_setzero_si64());

  v4sf sign_bit, poly_mask;
  COPY_MM_TO_XMM(mm0, mm1, sign_bit);
  COPY_MM_TO_XMM(mm2, mm3, poly_mask);
  _mm_empty(); /* good-bye mmx */
#endif
  /* The magic pass: "Extended precision modular arithmetic" 
     x = ((x - y * DP1) - y * DP2) - y * DP3; */
  xmm1 = *(v4sf*)_ps_minus_cephes_DP1;
  xmm2 = *(v4sf*)_ps_minus_cephes_DP2;
  xmm3 = *(v4sf*)_ps_minus_cephes_DP3;
  xmm1 = _mm_mul_ps(y, xmm1);
  xmm2 = _mm_mul_ps(y, xmm2);
  xmm3 = _mm_mul_ps(y, xmm3);
  x = _mm_add_ps(x, xmm1);
  x = _mm_add_ps(x, xmm2);
  x = _mm_add_ps(x, xmm3);
  
  /* Evaluate the first polynom  (0 <= x <= Pi/4) */
  y = *(v4sf*)_ps_coscof_p0;
  v4sf z = _mm_mul_ps(x,x);

  y = _mm_mul_ps(y, z);
  y = _mm_add_ps(y, *(v4sf*)_ps_coscof_p1);
  y = _mm_mul_ps(y, z);
  y = _mm_add_ps(y, *(v4sf*)_ps_coscof_p2);
  y = _mm_mul_ps(y, z);
  y = _mm_mul_ps(y, z);
  v4sf tmp = _mm_mul_ps(z, *(v4sf*)_ps_0p5);
  y = _mm_sub_ps(y, tmp);
  y = _mm_add_ps(y, *(v4sf*)_ps_1);
  
  /* Evaluate the second polynom  (Pi/4 <= x <= 0) */

  v4sf y2 = *(v4sf*)_ps_sincof_p0;
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_add_ps(y2, *(v4sf*)_ps_sincof_p1);
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_add_ps(y2, *(v4sf*)_ps_sincof_p2);
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_mul_ps(y2, x);
  y2 = _mm_add_ps(y2, x);

  /* select the correct result from the two polynoms */  
  xmm3 = poly_mask;
  y2 = _mm_and_ps(xmm3, y2); //, xmm3);
  y = _mm_andnot_ps(xmm3, y);
  y = _mm_add_ps(y,y2);
  /* update the sign */
  y = _mm_xor_ps(y, sign_bit);

  return y;
}

/* since sin_ps and cos_ps are almost identical, sincos_ps could replace both of them..
   it is almost as fast, and gives you a free cosine with your sine */
void sincos_ps(v4sf x, v4sf *s, v4sf *c) {
  v4sf xmm1, xmm2, xmm3 = _mm_setzero_ps(), sign_bit_sin, y;
#ifdef USE_SSE2
  v4si emm0, emm2, emm4;
#else
  v2si mm0, mm1, mm2, mm3, mm4, mm5;
#endif
  sign_bit_sin = x;
  /* take the absolute value */
  x = _mm_and_ps(x, *(v4sf*)_ps_inv_sign_mask);
  /* extract the sign bit (upper one) */
  sign_bit_sin = _mm_and_ps(sign_bit_sin, *(v4sf*)_ps_sign_mask);
  
  /* scale by 4/Pi */
  y = _mm_mul_ps(x, *(v4sf*)_ps_cephes_FOPI);
    
#ifdef USE_SSE2
  /* store the integer part of y in emm2 */
  emm2 = _mm_cvttps_epi32(y);

  /* j=(j+1) & (~1) (see the cephes sources) */
  emm2 = _mm_add_epi32(emm2, *(v4si*)_pi32_1);
  emm2 = _mm_and_si128(emm2, *(v4si*)_pi32_inv1);
  y = _mm_cvtepi32_ps(emm2);

  emm4 = emm2;

  /* get the swap sign flag for the sine */
  emm0 = _mm_and_si128(emm2, *(v4si*)_pi32_4);
  emm0 = _mm_slli_epi32(emm0, 29);
  v4sf swap_sign_bit_sin = _mm_castsi128_ps(emm0);

  /* get the polynom selection mask for the sine*/
  emm2 = _mm_and_si128(emm2, *(v4si*)_pi32_2);
  emm2 = _mm_cmpeq_epi32(emm2, _mm_setzero_si128());
  v4sf poly_mask = _mm_castsi128_ps(emm2);
#else
  /* store the integer part of y in mm2:mm3 */
  xmm3 = _mm_movehl_ps(xmm3, y);
  mm2 = _mm_cvttps_pi32(y);
  mm3 = _mm_cvttps_pi32(xmm3);

  /* j=(j+1) & (~1) (see the cephes sources) */
  mm2 = _mm_add_pi32(mm2, *(v2si*)_pi32_1);
  mm3 = _mm_add_pi32(mm3, *(v2si*)_pi32_1);
  mm2 = _mm_and_si64(mm2, *(v2si*)_pi32_inv1);
  mm3 = _mm_and_si64(mm3, *(v2si*)_pi32_inv1);

  y = _mm_cvtpi32x2_ps(mm2, mm3);

  mm4 = mm2;
  mm5 = mm3;

  /* get the swap sign flag for the sine */
  mm0 = _mm_and_si64(mm2, *(v2si*)_pi32_4);
  mm1 = _mm_and_si64(mm3, *(v2si*)_pi32_4);
  mm0 = _mm_slli_pi32(mm0, 29);
  mm1 = _mm_slli_pi32(mm1, 29);
  v4sf swap_sign_bit_sin;
  COPY_MM_TO_XMM(mm0, mm1, swap_sign_bit_sin);

  /* get the polynom selection mask for the sine */

  mm2 = _mm_and_si64(mm2, *(v2si*)_pi32_2);
  mm3 = _mm_and_si64(mm3, *(v2si*)_pi32_2);
  mm2 = _mm_cmpeq_pi32(mm2, _mm_setzero_si64());
  mm3 = _mm_cmpeq_pi32(mm3, _mm_setzero_si64());
  v4sf poly_mask;
  COPY_MM_TO_XMM(mm2, mm3, poly_mask);
#endif

  /* The magic pass: "Extended precision modular arithmetic" 
     x = ((x - y * DP1) - y * DP2) - y * DP3; */
  xmm1 = *(v4sf*)_ps_minus_cephes_DP1;
  xmm2 = *(v4sf*)_ps_minus_cephes_DP2;
  xmm3 = *(v4sf*)_ps_minus_cephes_DP3;
  xmm1 = _mm_mul_ps(y, xmm1);
  xmm2 = _mm_mul_ps(y, xmm2);
  xmm3 = _mm_mul_ps(y, xmm3);
  x = _mm_add_ps(x, xmm1);
  x = _mm_add_ps(x, xmm2);
  x = _mm_add_ps(x, xmm3);

#ifdef USE_SSE2
  emm4 = _mm_sub_epi32(emm4, *(v4si*)_pi32_2);
  emm4 = _mm_andnot_si128(emm4, *(v4si*)_pi32_4);
  emm4 = _mm_slli_epi32(emm4, 29);
  v4sf sign_bit_cos = _mm_castsi128_ps(emm4);
#else
  /* get the sign flag for the cosine */
  mm4 = _mm_sub_pi32(mm4, *(v2si*)_pi32_2);
  mm5 = _mm_sub_pi32(mm5, *(v2si*)_pi32_2);
  mm4 = _mm_andnot_si64(mm4, *(v2si*)_pi32_4);
  mm5 = _mm_andnot_si64(mm5, *(v2si*)_pi32_4);
  mm4 = _mm_slli_pi32(mm4, 29);
  mm5 = _mm_slli_pi32(mm5, 29);
  v4sf sign_bit_cos;
  COPY_MM_TO_XMM(mm4, mm5, sign_bit_cos);
  _mm_empty(); /* good-bye mmx */
#endif

  sign_bit_sin = _mm_xor_ps(sign_bit_sin, swap_sign_bit_sin);

  
  /* Evaluate the first polynom  (0 <= x <= Pi/4) */
  v4sf z = _mm_mul_ps(x,x);
  y = *(v4sf*)_ps_coscof_p0;

  y = _mm_mul_ps(y, z);
  y = _mm_add_ps(y, *(v4sf*)_ps_coscof_p1);
  y = _mm_mul_ps(y, z);
  y = _mm_add_ps(y, *(v4sf*)_ps_coscof_p2);
  y = _mm_mul_ps(y, z);
  y = _mm_mul_ps(y, z);
  v4sf tmp = _mm_mul_ps(z, *(v4sf*)_ps_0p5);
  y = _mm_sub_ps(y, tmp);
  y = _mm_add_ps(y, *(v4sf*)_ps_1);
  
  /* Evaluate the second polynom  (Pi/4 <= x <= 0) */

  v4sf y2 = *(v4sf*)_ps_sincof_p0;
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_add_ps(y2, *(v4sf*)_ps_sincof_p1);
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_add_ps(y2, *(v4sf*)_ps_sincof_p2);
  y2 = _mm_mul_ps(y2, z);
  y2 = _mm_mul_ps(y2, x);
  y2 = _mm_add_ps(y2, x);

  /* select the correct result from the two polynoms */  
  xmm3 = poly_mask;
  v4sf ysin2 = _mm_and_ps(xmm3, y2);
  v4sf ysin1 = _mm_andnot_ps(xmm3, y);
  y2 = _mm_sub_ps(y2,ysin2);
  y = _mm_sub_ps(y, ysin1);

  xmm1 = _mm_add_ps(ysin1,ysin2);
  xmm2 = _mm_add_ps(y,y2);
 
  /* update the sign */
  *s = _mm_xor_ps(xmm1, sign_bit_sin);
  *c = _mm_xor_ps(xmm2, sign_bit_cos);
}

// #pragma optimize("", on)
