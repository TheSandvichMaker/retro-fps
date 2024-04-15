// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#if 0
typedef struct hull_node_t
{
    struct hull_node_t *next;
    triangle_mesh_t mesh;
    hull_debug_t *debug;
} hull_node_t;

collision_geometry_t collision_geometry_from_map(arena_t *arena, map_t *map)
{
    collision_geometry_t result = {
        .hull_count = map->brush_count,
        .hulls      = m_alloc_array(arena, map->brush_count, collision_hull_t),
    };

    float player_height = 56.0f; // TODO: What about crouching?

    rect3_t b /* player_bounds */ = {
        .min = { -16, -16, -player_height },
        .max = {  16,  16,  0  },
    };
    arena_t emergency_temp = {0}; // oh no...

    m_scoped_temp(temp, arena)
    {
        hull_node_t *first_hull = NULL;
        hull_node_t *last_hull = NULL;

        size_t total_vertex_count = 0;

        for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++) 
        {
            map_brush_t *brush = &map->brushes[brush_index];

            uint32_t splat_vertex_count = 0;

            for (size_t poly_index = 0; poly_index < brush->plane_poly_count; poly_index++)
            {
                map_poly_t *poly = &map->polys[poly_index];
                splat_vertex_count += poly->index_count;
            }

            splat_vertex_count *= 8;

            v3_t *splat_vertices = m_alloc_array_nozero(temp_inner, brush->plane_poly_count, v3_t);

            size_t splat_index = 0;

            for (size_t poly_index = 0; poly_index < brush->plane_poly_count; poly_index++)
            {
                map_poly_t *poly = &map->polys[poly_index];

                uint32_t first_index = poly->first_index;
                uint32_t index_count = poly->index_count;

                for (size_t index_index = 0; index_index < index_count; index_index++)
                {
                    uint32_t index = first_index + map->indices[index_index];

                    v3_t vertex = map->vertex.positions[index];

                    splat_vertices[splat_index + 0] = add(vertex, make_v3(b.min.x, b.min.y, b.min.z));
                    splat_vertices[splat_index + 1] = add(vertex, make_v3(b.max.x, b.min.y, b.min.z));
                    splat_vertices[splat_index + 2] = add(vertex, make_v3(b.min.x, b.max.y, b.min.z));
                    splat_vertices[splat_index + 3] = add(vertex, make_v3(b.min.x, b.min.y, b.max.z));
                    splat_vertices[splat_index + 4] = add(vertex, make_v3(b.max.x, b.max.y, b.min.z));
                    splat_vertices[splat_index + 5] = add(vertex, make_v3(b.min.x, b.max.y, b.max.z));
                    splat_vertices[splat_index + 6] = add(vertex, make_v3(b.max.x, b.min.y, b.max.z));
                    splat_vertices[splat_index + 7] = add(vertex, make_v3(b.max.x, b.max.y, b.max.z));

                    splat_index += 8;
                }
            }

            hull_node_t *node = m_alloc_struct(temp, hull_node_t);
            node->debug = m_alloc_struct(arena, hull_debug_t);
            node->mesh = calculate_convex_hull_debug(temp, splat_vertex_count, splat_vertices, node->debug);
            convex_hull_do_extended_diagnostics(&node->mesh, node->debug);

            sll_push_back(first_hull, last_hull, node);

            total_vertex_count += 3*node->mesh.triangle_count;
        }

        result.vertex_count = total_vertex_count;
        result.vertices = m_alloc_array_nozero(arena, result.vertex_count, v3_t);

        size_t hull_vertex_index = 0;

        hull_node_t *hull_node = first_hull;
        for (size_t hull_index = 0; hull_index < result.hull_count; hull_index++)
        {
            collision_hull_t *hull = &result.hulls[hull_index];

            hull->count = (uint32_t)(3*hull_node->mesh.triangle_count); // TODO: checked truncate
            hull->first = (uint32_t)hull_vertex_index;
            hull->debug = hull_node->debug;

            copy_array(&result.vertices[hull->first], hull_node->mesh.points, hull->count);

            hull_vertex_index += hull->count;
            hull_node = hull_node->next;
        }
    }

    return result;
}
#endif
