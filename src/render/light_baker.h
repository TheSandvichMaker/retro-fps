#ifndef LIGHT_BAKER_H
#define LIGHT_BAKER_H

#include "core/api_types.h"

typedef struct light_bake_params_t
{
    arena_t *arena;
    struct map_t *map;

    v3_t sun_direction;
    v3_t sun_color;
    v3_t ambient_color;
} light_bake_params_t;

void bake_lighting(const light_bake_params_t *params);

#endif /* LIGHT_BAKER_H */
