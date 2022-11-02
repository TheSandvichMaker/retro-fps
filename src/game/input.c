#include "game.h"

static input_kind_t input_kind;

static uint64_t buttons_down;
static uint64_t buttons_pressed;
static uint64_t buttons_released;

static int mouse_x,  mouse_y;
static int mouse_dx, mouse_dy;

void update_input_state(const input_state_t *state)
{
    input_kind = state->input_kind;

    buttons_pressed  = (buttons_down^state->button_states) &  state->button_states;
    buttons_released = (buttons_down^state->button_states) & ~state->button_states;
    buttons_down     = state->button_states;

    mouse_x = state->mouse_x;
    mouse_y = state->mouse_y;

    mouse_dx = state->mouse_dx;
    mouse_dy = state->mouse_dy;
}

input_kind_t get_input_kind(void)
{
    return input_kind;
}

bool button_pressed(uint64_t buttons)
{
    return !!(buttons_pressed & buttons);
}

bool button_released(uint64_t buttons)
{
    return !!(buttons_released & buttons);
}

bool button_down(uint64_t buttons)
{
    return !!(buttons_down & buttons);
}

void get_mouse(int *x, int *y)
{
    *x = mouse_x;
    *y = mouse_y;
}

void get_mouse_delta(int *dx, int *dy)
{
    *dx = mouse_dx;
    *dy = mouse_dy;
}
