#ifndef GEOMETRIC_ALGEBRA_H
#define GEOMETRIC_ALGEBRA_H

#include "api_types.h"

//
// basis planes:
//
// xy
// zx
// yz
//
// From Marc ten Bosch:
// [Aside] I chose a lexicographic order for the basis because it is easy to remember, 
// but choosing z∧x instead of x∧z would make the signs the same. It would also makes 
// the bivector basis directions consistent. This is the right hand rule, except the 
// understandable non-arbitrary version :)
//  

typedef struct rotor2_t
{
    float a, b;
} rotor2_t;

// TODO: Implement rotor2

typedef struct biv3_t
{
    float xy, zx, yz;
} biv3_t;

typedef struct rotor3_t
{
    // scalar part
    float a;
    // bivector part
    union
    {
        struct
        {
            float xy;
            float zx;
            float yz;
        };
        biv3_t b;
    };
} rotor3_t;

static inline biv3_t wedge3(v3_t a, v3_t b)
{
    biv3_t result = {
        a.x*b.y - b.x*a.y, // xy
        a.z*b.x - b.z*a.x, // zx
        a.y*b.z - b.y*a.z, // yz
    };
    return result;
}

static biv3_t PLANE_XY = { 1.0f, 0.0f, 0.0f };
static biv3_t PLANE_ZX = { 0.0f, 1.0f, 0.0f };
static biv3_t PLANE_YZ = { 0.0f, 0.0f, 1.0f };

static inline float rotor3_lengthsq(rotor3_t r)
{
    return r.a*r.a + r.xy*r.xy + r.zx*r.zx + r.yz*r.yz;
}

static inline float rotor3_length(rotor3_t r)
{
    return sqrt_ss(rotor3_lengthsq(r));
}

static inline void rotor3_normalize_in_place(rotor3_t *r)
{
    float l = rotor3_length(*r);
    r->a  /= l;
    r->xy /= l;
    r->zx /= l;
    r->yz /= l;
}

static inline rotor3_t rotor3_normalize(rotor3_t r)
{
    rotor3_t result = r;
    rotor3_normalize_in_place(&result);
    return result;
}

static inline rotor3_t rotor3_from_vectors(v3_t from, v3_t to)
{
    rotor3_t r;
    r.a = 1.0f + dot(to, from);

    biv3_t minus_b = wedge3(to, from);
    r.xy = minus_b.xy;
    r.zx = minus_b.zx;
    r.yz = minus_b.yz;

    rotor3_normalize_in_place(&r);

    return r;
}

// plane must be normalized
static inline rotor3_t rotor3_from_plane_angle(biv3_t plane, float angle_radians)
{
    float sin_a, cos_a;
    sincos_ss(0.5f*angle_radians, &sin_a, &cos_a);

    rotor3_t r;
    r.a = cos_a;

    r.xy = -sin_a*plane.xy;
    r.zx = -sin_a*plane.zx;
    r.yz = -sin_a*plane.yz;

    return r;
}

static inline rotor3_t rotor3_reverse(rotor3_t r)
{
    rotor3_t result = {
        .a   = r.a,
        .xy = -r.xy,
        .zx = -r.zx,
        .yz = -r.yz,
    };
    return result;
}

static inline rotor3_t rotor3_product(rotor3_t p, rotor3_t q)
{
    rotor3_t r;

    r.a = p.a*q.a - p.xy*q.xy - p.zx*q.zx - p.yz*q.yz;

    r.xy = p.xy * q.a  + p.a  * q.xy
         + p.yz * q.zx - p.zx * q.yz;

    r.zx = p.zx * q.a  + p.a  * q.zx
         - p.yz * q.xy + p.xy * q.yz;

    r.yz = p.yz * q.a  + p.a  * q.yz
         + p.zx * q.xy - p.xy * q.zx;

    return r;
}

static inline v3_t rotor3_rotatev(rotor3_t p, v3_t v)
{
#if 0
    // q = P x
    v3_t q;
    q.x = p.a*v.x + v.y*p.xy + v.z*p.zx;
    q.y = p.a*v.y - v.x*p.xy + v.z*p.yz;
    q.z = p.a*v.z - v.x*p.xy - v.y*p.yz;

    float qxyz = v.x*p.yz - v.y*p.zx + v.z*p.xy; // trivector (TODO: What??)

    // r = q P*
    v3_t r;
    r.x = p.a*q.x + q.y *p.xy + q.z *p.zx + qxyz*p.yz;
    r.y = p.a*q.y - q.x *p.xy - qxyz*p.zx + q.z *p.yz;
    r.z = p.a*q.z + qxyz*p.xy - q.x *p.zx + q.y *p.yz;

    // trivector part of the result is always zero

    return r;
#else
    rotor3_t q = { 0.0f, v.x, v.y, v.z };

    rotor3_t r;
    r = rotor3_product(p, q);
    r = rotor3_product(r, rotor3_reverse(p));

    v3_t result = {
        r.xy,
        r.zx,
        r.yz,
    };

    return result;
#endif
}

static inline rotor3_t rotor3_rotate(rotor3_t q, rotor3_t r)
{
    return rotor3_product(rotor3_product(q, r), rotor3_reverse(q));
}

#endif /* GEOMETRIC_ALGEBRA_H */
