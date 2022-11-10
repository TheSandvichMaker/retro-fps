#ifndef RETRO_H
#define RETRO_H

#include "core/api_types.h"
#include "input.h"

typedef struct map_t map_t;
typedef struct map_brush_t map_brush_t;

typedef struct game_io_t
{
    // in
    bool has_focus;

    const input_state_t *input_state;

    // out
    bool cursor_locked;
    bool exit_requested;
} game_io_t;

void game_tick(game_io_t *io, float dt);

typedef enum player_move_mode_e
{
    PLAYER_MOVE_NORMAL,
    PLAYER_MOVE_FREECAM,
} player_move_mode_e;

typedef struct player_t
{
    player_move_mode_e move_mode;
    map_brush_t *support;

    v3_t p;
    v3_t dp;
    float look_pitch;
    float look_yaw;

    bool crouched;
    float crouch_t;
} player_t;

typedef struct world_t
{
    arena_t arena;
    map_t *map;

    player_t *player;
} world_t;

#endif /* RETRO_H */
