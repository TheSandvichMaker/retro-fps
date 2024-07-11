#pragma once

meta_struct
typedef struct worldspawn_t
{
    meta(default = { 1, 1, 1 }, ui_type = "color_picker")
    v3_t sun_color;

    meta(default = 1.0, ui_min = 0.0, ui_max = 10.0)
    float sun_brightness;

    meta(default = 0.002, ui_min = 0.0, ui_max = 0.1)
    float fog_absorption;

    meta(default = 0.02, ui_min = 0.0, ui_max = 0.1)
    float fog_density;

    meta(default = 0.04, ui_min = 0.0, ui_max = 0.1)
    float fog_scattering;

    meta(default = 0.6, ui_min = -0.9, ui_max = 0.9)
    float fog_phase;

    meta(default = { 0, 0, 0 }, ui_type = "color_picker")
    v3_t fog_ambient_inscattering;
} worldspawn_t;

fn void worldspawn_deserialize(worldspawn_t *dst, struct map_t *map, struct map_entity_t *src);
