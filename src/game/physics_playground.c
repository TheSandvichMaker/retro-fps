// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

global phys_scene_t g_phys_scene;
global bool g_phys_scene_initialized = false;

v3_t phys_get_center_of_mass_world(phys_body_t *body)
{
    v3_t cm          = body->shape->center_of_mass;
    v3_t cm_oriented = quat_rotatev(body->orientation, cm);
    v3_t cm_world    = add(cm_oriented, body->position);
    return cm_world;
}

void phys_apply_impulse_linear(phys_body_t *body, v3_t impulse)
{
    body->linear_velocity = add(body->linear_velocity, mul(impulse, body->inv_mass));
}

void phys_apply_impulse_angular(phys_body_t *body, v3_t impulse)
{
    if (body->inv_mass == 0.0f)
    {
        return;
    }

    m3x3_t inv_inertia_tensor = phys_get_inv_inertia_tensor_world(body);
    body->angular_velocity = add(body->angular_velocity, mul(inv_inertia_tensor, impulse));

    float max_angular_speed = 30.0f; // radians per second
    if (vlensq(body->angular_velocity) > max_angular_speed*max_angular_speed)
    {
        body->angular_velocity = normalize(body->angular_velocity);
        body->angular_velocity = mul(body->angular_velocity, max_angular_speed);
    }
}

void phys_apply_impulse(phys_body_t *body, v3_t impulse_point, v3_t impulse)
{
    if (body->inv_mass == 0.0f)
    {
        return;
    }

    phys_apply_impulse_linear(body, impulse);
    v3_t position = phys_get_center_of_mass_world(body);
    v3_t r = sub(impulse_point, position);
    v3_t impulse_angular = cross(r, impulse); // dL -> change in angular momentum == angular impulse
    phys_apply_impulse_angular(body, impulse_angular);
}

bool phys_intersect(phys_body_t *body_a, phys_body_t *body_b, phys_contact_t *contact)
{
    v3_t ab = sub(body_b->position, body_a->position);

    v3_t normal = normalize_or_zero(ab);

    phys_sphere_t *sphere_a = (phys_sphere_t *)body_a->shape;
    phys_sphere_t *sphere_b = (phys_sphere_t *)body_b->shape;

    float radius_ab = sphere_a->radius + sphere_b->radius;
    float length_sq = vlen_sq(ab);

    contact->body_a = body_a;
    contact->body_b = body_b;
    contact->normal = normal;
    v3_t p_on_a_local = mul( sphere_a->radius, normal);
    v3_t p_on_b_local = mul(-sphere_b->radius, normal);
    contact->p_on_a_local = p_on_a_local;
    contact->p_on_b_local = p_on_b_local;
    contact->p_on_a_world = add(body_a->position, p_on_a_local);
    contact->p_on_b_world = add(body_b->position, p_on_b_local);

    bool result = length_sq <= radius_ab*radius_ab;
    return result;
}

fn_local void phys_resolve_contact(const phys_contact_t *contact)
{
    phys_body_t *body_a = contact->body_a;
    phys_body_t *body_b = contact->body_b;

    float inv_mass_a = body_a->inv_mass;
    float inv_mass_b = body_b->inv_mass;

    float elasticity_a = body_a->elasticity;
    float elasticity_b = body_b->elasticity;
    float elasticity = elasticity_a*elasticity_b;

    v3_t n = contact->normal;
    v3_t vab = sub(body_a->linear_velocity, body_b->linear_velocity);
    float impulse_j = -(1.0f + elasticity)*dot(vab, n) / (inv_mass_a + inv_mass_b);
    v3_t vector_impulse_j = mul(n, impulse_j);

    phys_apply_impulse_linear(body_a, vector_impulse_j);
    phys_apply_impulse_linear(body_b, negate(vector_impulse_j));

    float t_a = body_a->inv_mass / (body_a->inv_mass + body_b->inv_mass);
    float t_b = body_b->inv_mass / (body_a->inv_mass + body_b->inv_mass);

    v3_t ds = sub(contact->p_on_b_world, contact->p_on_a_world);
    body_a->position = add(body_a->position, mul(ds, t_a));
    body_b->position = sub(body_b->position, mul(ds, t_b));
}

m3x3_t phys_get_inv_inertia_tensor_world(phys_body_t *body)
{
    m3x3_t result = body->shape->inv_inertia_tensor;
    m3x3_t orient = m3x3_from_quat(body->orientation);
    m3x3_t orient_transpose = m3x3_transpose(orient);
    result = mul(orient, mul(result, orient_transpose));
    return result;
}

void phys_body_update(phys_body_t *body, float dt)
{
    phys_shape_t *shape = body->shape;

    body->position = add(body->position, mul(body->linear_velocity, dt));

    v3_t position_cm = phys_get_center_of_mass_world(body);
    v3_t cm_to_pos   = sub(body->position, position_cm);

    m3x3_t orientation = m3x3_from_quat(body->orientation);
    m3x3_t inertia_tensor = mul(mul(orientation, shape->inertia_tensor), m3x3_transpose(orientation));
    v3_t alpha = mul(m3x3_inverse(inertia_tensor), 
                     cross(body->angular_velocity, mul(inertia_tensor, body->angular_velocity)));
    body->angular_velocity = add(body->angular_velocity, mul(alpha, dt));

    v3_t d_angle = mul(body->angular_velocity, dt);
    quat_t dq = quat_from_axis_angle(d_angle, vlen(d_angle));
    body->orientation = quat_mul(dq, body->orientation);
    body->orientation = normalize(body->orientation);

    body->position = add(position_cm, quat_rotatev(dq, cm_to_pos));

    // jesus christ... next, add angular impulses to contact resolution
    
}

fn_local phys_body_t *phys_add_body(phys_scene_t *scene, phys_shape_t *shape)
{
    phys_body_t *result = NULL;
    if (ALWAYS(scene->bodies_count < MAX_PHYS_BODIES))
    {
        result = &scene->bodies[scene->bodies_count++];
        zero_struct(result);
        result->orientation = QUAT_IDENTITY;
        result->shape       = shape;
    }
    return result;
}

fn_local void phys_init_sphere(phys_sphere_t *sphere, float radius)
{
    zero_struct(sphere);
    sphere->kind   = PHYS_SHAPE_SPHERE;
    sphere->radius = radius;

    sphere->inertia_tensor.e[0][0] = 2.0f*sphere->radius*sphere->radius / 5.0f;
    sphere->inertia_tensor.e[1][1] = 2.0f*sphere->radius*sphere->radius / 5.0f;
    sphere->inertia_tensor.e[2][2] = 2.0f*sphere->radius*sphere->radius / 5.0f;

    sphere->inv_inertia_tensor.e[0][0] = 1.0f / (2.0f*sphere->radius*sphere->radius / 5.0f);
    sphere->inv_inertia_tensor.e[1][1] = 1.0f / (2.0f*sphere->radius*sphere->radius / 5.0f);
    sphere->inv_inertia_tensor.e[2][2] = 1.0f / (2.0f*sphere->radius*sphere->radius / 5.0f);
}

fn_local void initialize_phys_scene(phys_scene_t *scene, gamestate_t *game)
{
    camera_t *camera = game->primary_camera;
    ASSERT(camera);

    camera->p = (v3_t){ -10, 0, 5 };

    {
        phys_sphere_t *sphere = m_alloc_struct(&scene->arena, phys_sphere_t);
        phys_init_sphere(sphere, 1.0f);

        phys_body_t *body = phys_add_body(scene, (phys_shape_t *)sphere);
        body->inv_mass   = 1.0f;
        body->elasticity = 0.5f;
        body->position   = (v3_t){ 0, 0, 10 };
    }

    {
        phys_sphere_t *sphere = m_alloc_struct(&scene->arena, phys_sphere_t);
        phys_init_sphere(sphere, 1000.0f);

        phys_body_t *body = phys_add_body(scene, (phys_shape_t *)sphere);
        body->inv_mass   = 0.0f;
        body->elasticity = 1.0f;
        body->position   = (v3_t){ 0, 0, -1000.0f };
    }

    g_phys_scene_initialized = true;
}

void update_and_render_physics_playground(r_context_t *rc, gamestate_t *game, float dt)
{
    phys_scene_t *scene = &g_phys_scene;

    if (!g_phys_scene_initialized)
    {
        initialize_phys_scene(scene, game);
    }

    for (size_t body_index = 0; body_index < scene->bodies_count; body_index++)
    {
        phys_body_t *body = &scene->bodies[body_index];

        if (body->inv_mass != 0.0f)
        {
            v3_t impulse_gravity = { 0, 0, -dt*10.0f / body->inv_mass };
            phys_apply_impulse_linear(body, impulse_gravity);
        }
    }

    for (size_t body_a_index = 0; body_a_index < scene->bodies_count; body_a_index++)
    {
        for (size_t body_b_index = body_a_index + 1; body_b_index < scene->bodies_count; body_b_index++)
        {
            phys_body_t *body_a = &scene->bodies[body_a_index];
            phys_body_t *body_b = &scene->bodies[body_b_index];

            if (body_a->inv_mass == 0.0f &&
                body_b->inv_mass == 0.0f)
            {
                continue;
            }

            phys_contact_t contact;
            if (phys_intersect(body_a, body_b, &contact))
            {
                phys_resolve_contact(&contact);
            }
        }
    }

    for (size_t body_index = 0; body_index < scene->bodies_count; body_index++)
    {
        phys_body_t *body = &scene->bodies[body_index];
        phys_body_update(body, dt);
    }

    //
    //
    //

    camera_t *camera = game->primary_camera;
    ASSERT(camera);

    if (g_cursor_locked)
    {
        camera_freecam(camera, 0.25f, dt);
    }

    //
    // render
    //

#if 0
    int res_x, res_y;
    render->get_resolution(&res_x, &res_y);
#else
	int res_x = 1920;
	int res_y = 1080;
#endif

    rect2_t viewport = {
        0, 0, (float)res_x, (float)res_y,
    };

    r_view_t view;
    init_view_for_camera(camera, viewport, &view);

    R_VIEW(rc, r_make_view(rc, &view))
    {
        for (size_t body_index = 0; body_index < scene->bodies_count; body_index++)
        {
            // NOTE: immediate flushing in the loop because of the transform
            // transform is only there to show rotation.

            R_IMMEDIATE(rc, imm)
            {
                phys_body_t   *body   = &scene->bodies[body_index];
                phys_sphere_t *sphere = (phys_sphere_t *)body->shape;

                imm->topology  = R_TOPOLOGY_TRIANGLELIST;
                imm->shader    = R_SHADER_DEBUG_LIGHTING;
                imm->use_depth = true;

                m4x4_t translation = m4x4_from_translation(body->position);
                m4x4_t rotation    = m4x4_from_quat(body->orientation);
                m4x4_t scale       = m4x4_from_sscale(sphere->radius);
                m4x4_t transform   = mul(translation, mul(rotation, scale));
                imm->transform = transform;

                r_immediate_sphere(rc, V3_ZERO, 1.0f, COLORF_WHITE, 64, 64);
            }
        }
    }
}
