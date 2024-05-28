// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct gamestate_t
{
	arena_t arena;

    map_t        *map;
	map_entity_t *worldspawn;

    camera_t *primary_camera;
    player_t *player;

    float fade_t;
    float fade_target_t;
} gamestate_t;
