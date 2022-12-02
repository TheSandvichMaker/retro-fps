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

typedef struct game_audio_io_t
{
    size_t frames_to_mix;
    float *buffer;
} game_audio_io_t;

void game_tick(game_io_t *io, float dt);
void game_mix_audio(game_audio_io_t *audio_io);

typedef enum player_move_mode_t
{
    PLAYER_MOVE_NORMAL,
    PLAYER_MOVE_FREECAM,
} player_move_mode_t;

typedef struct camera_t
{
    v3_t p;

    float distance;
    float pitch;
    float yaw;
    float roll;

    float vfov; // degrees

    // axes computed from the pitch, yaw, roll values
    v3_t computed_x;
    v3_t computed_y;
    v3_t computed_z;
} camera_t;

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

    player_t *player;
} world_t;

v3_t player_view_origin(player_t *player);
v3_t player_view_direction(player_t *player);

#endif /* RETRO_H */
