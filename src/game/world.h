// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct world_t
{
    arena_t arena;
    struct map_t *map;

    struct camera_t *primary_camera;
    player_t *player;

    float fade_t;
    float fade_target_t;
} world_t;
