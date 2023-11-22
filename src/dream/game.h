#ifndef RETRO_H
#define RETRO_H

#include "core/api_types.h"

typedef struct map_t map_t;
typedef struct map_brush_t map_brush_t;
typedef struct camera_t camera_t;

typedef enum player_move_mode_t
{
    PLAYER_MOVE_NORMAL,
    PLAYER_MOVE_FREECAM,
} player_move_mode_t;

typedef struct player_t
{
    camera_t *attached_camera;

    player_move_mode_t move_mode;
    map_brush_t *support;

    v3_t p;
    v3_t dp;

    bool crouched;
    float crouch_t;
} player_t;

typedef struct world_t
{
    arena_t arena;
    map_t *map;

    camera_t *primary_camera;
    player_t *player;

    float fade_t;
    float fade_target_t;
} world_t;

DREAM_LOCAL v3_t player_view_origin(player_t *player);
DREAM_LOCAL v3_t player_view_direction(player_t *player);
DREAM_LOCAL void player_freecam(player_t *player, float dt);

// frown
DREAM_LOCAL bool g_cursor_locked;

#endif /* RETRO_H */
