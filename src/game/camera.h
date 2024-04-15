#ifndef CAMERA_H
#define CAMERA_H

#include "core/api_types.h"

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

fn void compute_camera_axes(camera_t *camera);
fn void update_camera_rotation(camera_t *camera, float dt);
fn void camera_freecam(camera_t *camera, float move_speed, float dt);

#endif /* CAMERA_H */
