#ifndef DREAM_COLLISION_GEOMETRY_H
#define DREAM_COLLISION_GEOMETRY_H

#include "core/api_types.h"

typedef struct collision_hull_t
{
    uint32_t count;
    uint32_t first;

    struct hull_debug_t *debug;
} collision_hull_t;

typedef struct collision_geometry_t
{
    size_t vertex_count;
    v3_t  *vertices;

    size_t hull_count;
    collision_hull_t *hulls;
} collision_geometry_t;

fn collision_geometry_t collision_geometry_from_map(arena_t *arena, struct map_t *map);

#endif /* COLLISION_GEOMETRY_H */
