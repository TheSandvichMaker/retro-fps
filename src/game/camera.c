// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

void compute_camera_axes(camera_t *camera)

{
    v3_t forward = forward_vector_from_pitch_yaw(camera->pitch, camera->yaw);
    basis_vectors(forward, make_v3(0, 0, 1), &camera->computed_x, &camera->computed_y, &camera->computed_z);
}

void update_camera_rotation(camera_t *camera, float dt)
{
	(void)dt;

	v2_t dp = {0, 0}; // @ActionSystem get_action_mouse_dp();

    float look_speed_x = 0.1f;
    float look_speed_y = 0.1f;

    camera->yaw   -= look_speed_x*dp.x;
    camera->pitch -= look_speed_y*dp.y;

    camera->yaw   = fmodf(camera->yaw, 360.0f);
    camera->pitch = CLAMP(camera->pitch, -85.0f, 85.0f);

    compute_camera_axes(camera);
}

void camera_freecam(camera_t *camera, float move_speed, float dt)
{
    update_camera_rotation(camera, dt);

    v3_t move_delta = { 0 };

    if (action_held(Action_run))      move_speed *= 2.0f;

    if (action_held(Action_forward))  move_delta.x += move_speed;
    if (action_held(Action_back))     move_delta.x -= move_speed;
    if (action_held(Action_left))     move_delta.y += move_speed;
    if (action_held(Action_right))    move_delta.y -= move_speed;

    rotor3_t r_pitch = rotor3_from_plane_angle(PLANE_ZX, DEG_TO_RAD*camera->pitch);
    rotor3_t r_yaw   = rotor3_from_plane_angle(PLANE_YZ, DEG_TO_RAD*camera->yaw  );

    move_delta = rotor3_rotatev(r_pitch, move_delta);
    move_delta = rotor3_rotatev(r_yaw  , move_delta);

    if (action_held(Action_jump))     move_delta.z += move_speed;
    if (action_held(Action_crouch))   move_delta.z -= move_speed;

    camera->p = add(camera->p, move_delta);
}
