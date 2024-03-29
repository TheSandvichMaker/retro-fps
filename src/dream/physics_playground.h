#ifndef DREAM_PHYS_PLAYGROUND_H
#define DREAM_PHYS_PLAYGROUND_H

#include "core/api_types.h"

typedef enum phys_shape_kind_t
{
    PHYS_SHAPE_SPHERE,
} phys_shape_kind_t;

typedef struct phys_shape_t
{
    phys_shape_kind_t kind;
    v3_t center_of_mass;
    m3x3_t inertia_tensor;
    m3x3_t inv_inertia_tensor;
} phys_shape_t;

typedef struct phys_sphere_t
{
    union { phys_shape_t; phys_shape_t shape; };
    float radius;
} phys_sphere_t;

typedef struct phys_body_t
{
    v3_t     position;
    quat_t   orientation;
    v3_t     linear_velocity;
    v3_t     angular_velocity;

    float    inv_mass;
    float    elasticity;
    phys_shape_t *shape;
} phys_body_t;

DREAM_LOCAL v3_t phys_get_center_of_mass_world(phys_body_t *body);
DREAM_LOCAL m3x3_t phys_get_inv_inertia_tensor_world(phys_body_t *body);
DREAM_LOCAL void phys_body_update(phys_body_t *body, float dt);

typedef struct phys_contact_t
{
    v3_t p_on_a_world;
    v3_t p_on_b_world;
    v3_t p_on_a_local;
    v3_t p_on_b_local;
    
    v3_t normal; // worldspace
    float separation_distance; // positive when non-penetrating, negative when penetrating
    float time_of_impact;

    phys_body_t *body_a;
    phys_body_t *body_b;
} phys_contact_t;

DREAM_LOCAL void phys_apply_impulse_linear(phys_body_t *body, v3_t impulse);
DREAM_LOCAL void phys_apply_impulse_angular(phys_body_t *body, v3_t impulse);
DREAM_LOCAL void phys_apply_impulse(phys_body_t *body, v3_t impulse_point, v3_t impulse);

DREAM_LOCAL bool phys_intersect(phys_body_t *body_a, phys_body_t *body_b, phys_contact_t *out_contact);

#define MAX_PHYS_BODIES (4096)

typedef struct phys_scene_t
{
    arena_t     arena;

    size_t      bodies_count;
    phys_body_t bodies[MAX_PHYS_BODIES];
} phys_scene_t;

DREAM_LOCAL void update_and_render_physics_playground(struct r_context_t *rc, struct world_t *world, float dt);

#endif
