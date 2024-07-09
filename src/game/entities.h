#pragma once

typedef struct worldspawn_t
{
    v3_t  sun_color;
    float sun_brightness;
    float fog_absorption;
    float fog_density;
    float fog_scattering;
    float fog_phase;
    v3_t  fog_ambient_inscattering;
} worldspawn_t;

fn void worldspawn_deserialize(worldspawn_t *dst, struct map_t *map, struct map_entity_t *src);
