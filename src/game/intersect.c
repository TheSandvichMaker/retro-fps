// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

// TODO: Figure out compiler-independent warning disabling
#pragma warning(push)
#pragma warning(disable: 4723) // warns against divide-by-0. But dividing by 0 in this code behaves correctly, so that's ok.
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
#pragma warning(pop)

// TODO: Figure out compiler-independent warning disabling
#pragma warning(push)
#pragma warning(disable: 4723) // warns against divide-by-0. But dividing by 0 in this code behaves correctly, so that's ok.
bool ray_intersect_rect3_bvh(v3_t o, v3_t d, rect3_t rect, float max_t)
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

    return (t_max >= t_min) && (t_min <= max_t);
}
#pragma warning(pop)

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

bool intersect_map(map_t *map, const intersect_params_t *params, intersect_result_t *result)
{
    float min_t = params->min_t;
    float max_t = params->max_t;

    if (max_t == 0.0f)
        max_t = FLT_MAX;

    float t = max_t;

    v3_t o = params->o;
    v3_t d = params->d;

    v3_t hit_uvw = {0};
    uint32_t hit_triangle_offset = 0;

    map_brush_t *hit_brush = NULL;
    map_plane_t *hit_plane = NULL;
    map_poly_t  *hit_poly  = NULL;

    bool d_is_negative[3] = {
        d.x < 0.0f,
        d.y < 0.0f,
        d.z < 0.0f,
    };

    uint32_t node_stack_at = 0;
    uint32_t node_stack[64];

    node_stack[node_stack_at++] = 0;

    while (node_stack_at > 0)
    {
        uint32_t node_index = node_stack[--node_stack_at];

        map_bvh_node_t *node = &map->nodes[node_index];

        if (ray_intersect_rect3_bvh(o, d, node->bounds, t))
        {
            if (node->count > 0)
            {
                uint32_t first = node->left_first;
                uint16_t count = node->count;

                for (size_t brush_index = 0; brush_index < count; brush_index++)
                {
                    map_brush_t *brush = &map->brushes[first + brush_index];

                    bool ignored = false;
                    for (size_t ignored_brush_index = 0; ignored_brush_index < params->ignore_brush_count; ignored_brush_index++)
                    {
                        if (params->ignore_brushes[ignored_brush_index] == brush)
                        {
                            ignored = true;
                            break;
                        }
                    }

                    if (ignored)
                        continue;

                    for (size_t poly_index = 0; poly_index < brush->plane_poly_count; poly_index++)
                    {
                        map_poly_t *poly = &map->polys[brush->first_plane_poly + poly_index];

                        uint16_t *indices   = map->indices          + poly->first_index;
                        v3_t     *positions = map->vertex.positions + poly->first_vertex;

                        uint32_t triangle_count = poly->index_count / 3;
                        for (size_t triangle_index = 0; triangle_index < triangle_count; triangle_index++)
                        {
                            v3_t a = positions[indices[3*triangle_index + 0]];
                            v3_t b = positions[indices[3*triangle_index + 1]];
                            v3_t c = positions[indices[3*triangle_index + 2]];

                            v3_t uvw;
                            float triangle_hit_t = ray_intersect_triangle(o, d, a, b, c, &uvw);

                            if (triangle_hit_t >= min_t &&
                                triangle_hit_t < t)
                            {
                                t                   = triangle_hit_t;
                                hit_brush           = brush;
                                hit_plane           = &map->planes[brush->first_plane_poly + poly_index];
                                hit_poly            = poly;
                                hit_triangle_offset = (uint32_t)(3*triangle_index);
                                hit_uvw             = uvw;

                                if (params->occlusion_test)
                                    goto early_exit;
                            }
                        }
                    }
                }
            }
            else
            {
                uint32_t left = node->left_first;

                if (d_is_negative[node->split_axis])
                {
                    node_stack[node_stack_at++] = left;
                    node_stack[node_stack_at++] = left + 1;
                }
                else
                {
                    node_stack[node_stack_at++] = left + 1;
                    node_stack[node_stack_at++] = left;
                }
            }
        }
    }

early_exit:

    if (result)
    {
        result->t               = t;
        result->brush           = hit_brush;
        result->plane           = hit_plane;
        result->poly            = hit_poly;
        result->triangle_offset = hit_triangle_offset;
        result->uvw             = hit_uvw;
    }
    
    return t < max_t;
}
