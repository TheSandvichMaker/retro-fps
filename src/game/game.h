#ifndef RETRO_H
#define RETRO_H

#include "core/core.h"

typedef struct map_t map_t;
typedef struct map_brush_t map_brush_t;

typedef enum button_t
{
    BUTTON_LEFT          = 1 << 0,
    BUTTON_RIGHT         = 1 << 1,
    BUTTON_FORWARD       = 1 << 2,
    BUTTON_BACK          = 1 << 3,
    BUTTON_JUMP          = 1 << 4,
    BUTTON_RUN           = 1 << 5,
    BUTTON_CROUCH        = 1 << 6,
    BUTTON_FIRE1         = 1 << 7,
    BUTTON_FIRE2         = 1 << 8,
    BUTTON_NEXT_WEAPON   = 1 << 9,
    BUTTON_PREV_WEAPON   = 1 << 10,
    BUTTON_WEAPON1       = 1 << 11,
    BUTTON_WEAPON2       = 1 << 12,
    BUTTON_WEAPON3       = 1 << 13,
    BUTTON_WEAPON4       = 1 << 14,
    BUTTON_WEAPON5       = 1 << 15,
    BUTTON_WEAPON6       = 1 << 16,
    BUTTON_WEAPON7       = 1 << 17,
    BUTTON_WEAPON8       = 1 << 18,
    BUTTON_WEAPON9       = 1 << 19,
    BUTTON_ESCAPE        = 1 << 20,
    BUTTON_TOGGLE_NOCLIP = 1 << 21,
} button_t;

typedef enum axis_t
{
    AXIS_MOUSE_ABSOLUTE,
    AXIS_MOUSE_RELATIVE,
    AXIS_LSTICK_X,
    AXIS_LSTICK_Y,
    AXIS_RSTICK_X,
    AXIS_RSTICK_Y,
    AXIS_COUNT,
} axis_t;

typedef enum input_kind_t
{
    INPUT_KIND_KEYBOARD_MOUSE,
    INPUT_KIND_CONTROLLER,
} input_kind_t;

typedef struct input_state_t
{
    input_kind_t input_kind;

    int mouse_x,  mouse_y;
    int mouse_dx, mouse_dy;

    uint64_t button_states;
    float    axis_values[AXIS_COUNT];
} input_state_t;

void update_input_state(const input_state_t *state);

input_kind_t get_input_kind(void);

bool button_pressed (uint64_t buttons);
bool button_released(uint64_t buttons);
bool button_down    (uint64_t buttons);

void get_mouse      (int *x,  int *y);
void get_mouse_delta(int *dx, int *dy);

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
