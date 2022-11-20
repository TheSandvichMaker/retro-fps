#ifndef INPUT_H
#define INPUT_H

#include "core/api_types.h"

#define BUTTON_LEFT          (1 << 0)
#define BUTTON_RIGHT         (1 << 1)
#define BUTTON_FORWARD       (1 << 2)
#define BUTTON_BACK          (1 << 3)
#define BUTTON_JUMP          (1 << 4)
#define BUTTON_RUN           (1 << 5)
#define BUTTON_CROUCH        (1 << 6)
#define BUTTON_FIRE1         (1 << 7)
#define BUTTON_FIRE2         (1 << 8)
#define BUTTON_NEXT_WEAPON   (1 << 9)
#define BUTTON_PREV_WEAPON   (1 << 10)
#define BUTTON_WEAPON1       (1 << 11)
#define BUTTON_WEAPON2       (1 << 12)
#define BUTTON_WEAPON3       (1 << 13)
#define BUTTON_WEAPON4       (1 << 14)
#define BUTTON_WEAPON5       (1 << 15)
#define BUTTON_WEAPON6       (1 << 16)
#define BUTTON_WEAPON7       (1 << 17)
#define BUTTON_WEAPON8       (1 << 18)
#define BUTTON_WEAPON9       (1 << 19)
#define BUTTON_ESCAPE        (1 << 20)
#define BUTTON_TOGGLE_NOCLIP (1 << 21)
#define BUTTON_F1            (1 << 22)
#define BUTTON_F2            (1 << 23)
#define BUTTON_F3            (1 << 24)
#define BUTTON_F4            (1 << 25)
#define BUTTON_F5            (1 << 26)
#define BUTTON_F6            (1 << 27)
#define BUTTON_F7            (1 << 28)
#define BUTTON_F8            (1 << 29)
#define BUTTON_F9            (1 << 30)
#define BUTTON_F10           (1 << 31)
#define BUTTON_F11           (1 << 32)
#define BUTTON_F12           (1 << 33)

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
    int scroll_dx, scroll_dy;

    uint64_t button_states;
    float    axis_values[AXIS_COUNT];
} input_state_t;

void update_input_state(const input_state_t *state);

input_kind_t get_input_kind(void);

// this seems silly, but I could just have seperate
// calls to query input for the UI that will always
// work, while the ones below can have their input
// eaten.
bool ui_button_pressed (uint64_t buttons);
bool ui_button_released(uint64_t buttons);
bool ui_button_down    (uint64_t buttons);

void suppress_game_input(bool suppress);

bool button_pressed (uint64_t buttons);
bool button_released(uint64_t buttons);
bool button_down    (uint64_t buttons);

void get_mouse      (int *x,  int *y);
void get_mouse_delta(int *dx, int *dy);

#endif /* INPUT_H */
