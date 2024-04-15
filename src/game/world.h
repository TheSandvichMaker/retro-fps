#ifndef DREAM_WORLD_H
#define DREAM_WORLD_H

typedef struct world_t
{
    arena_t arena;
    struct map_t *map;

    struct camera_t *primary_camera;
    player_t *player;

    float fade_t;
    float fade_target_t;
} world_t;

#endif
