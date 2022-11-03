#include <float.h>

//
//
//

#include "intersect.h"
#include "core/math.h"

//
//
//

float ray_intersect_rect3(v3_t o, v3_t d, rect3_t rect)
{
    v3_t inv_d = { 1.0f / d.x, 1.0f / d.y, 1.0f / d.z };

    float tx1 = inv_d.x*(rect.min.x - o.x);
    float tx2 = inv_d.x*(rect.max.x - o.x);

    float t_min = min(tx1, tx2);
    float t_max = max(tx1, tx2);

    float ty1 = inv_d.y*(rect.min.y - o.y);
    float ty2 = inv_d.y*(rect.max.y - o.y);

    t_min = max(t_min, min(ty1, ty2));
    t_max = min(t_max, max(ty1, ty2));

    float tz1 = inv_d.z*(rect.min.z - o.z);
    float tz2 = inv_d.z*(rect.max.z - o.z);

    t_min = max(t_min, min(tz1, tz2));
    t_max = min(t_max, max(tz1, tz2));

    float t = (t_min >= 0.0f ? t_min : t_max);

    if (t_max >= t_min) 
        return t;
    else                
        return FLT_MAX;
}

float ray_intersect_triangle(v3_t o, v3_t d, v3_t a, v3_t b, v3_t c, v3_t *uvw)
{
    float epsilon = 0.000000001f;

    v3_t edge1 = sub(b, a);
    v3_t edge2 = sub(c, a);

    v3_t pvec = cross(d, edge2);

    float det = dot(edge1, pvec);

    if (det > -epsilon && det < epsilon)
        return FLT_MAX;

    float rcp_det = 1.0f / det;

    v3_t tvec = sub(o, a);

    float v = rcp_det*dot(tvec, pvec);

    if (v < 0.0f || v > 1.0f)
        return FLT_MAX;

    v3_t qvec = cross(tvec, edge1);

    float w = rcp_det*dot(d, qvec);

    if (w < 0.0f || v + w > 1.0f)
        return FLT_MAX;

    float t = rcp_det*dot(qvec, edge2);

    if (t < epsilon)
        return FLT_MAX;

    if (uvw)
    {
        uvw->x = 1.0f - v - w;
        uvw->y = v;
        uvw->z = w;
    }

    return t;
}

v3_t get_normal_rect3(v3_t hit_p, rect3_t rect)
{
    v3_t p = mul(0.5f, add(rect.min, rect.max));
    v3_t r = mul(0.5f, sub(rect.max, rect.min));

    v3_t rel_p = div(sub(hit_p, p), r);

    int  e = 0;
    v3_t n = { abs_ss(rel_p.x), abs_ss(rel_p.y), abs_ss(rel_p.z) };

    if (n.y > n.e[e])  e = 1;
    if (n.z > n.e[e])  e = 2;

    v3_t result = { 0, 0, 0 };
    result.e[e] = sign_of(rel_p.e[e]);

    return result;
}
