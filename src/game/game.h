#pragma once

#include "asset.h"
#include "audio.h"
#include "bvh.h"
#include "camera.h"
#include "collision_geometry.h"
#include "convar.h"
#include "convex_hull_debugger.h"
#include "dream.h"
#include "font.h"
#include "freeverb.h"
#include "in_game_editor.h"
#include "input.h"
#include "intersect.h"
#include "job_queues.h"
#include "light_baker.h"
#include "log.h"
#include "map.h"
#include "mesh.h"
#include "physics_playground.h"
#include "render.h"
#include "render_backend.h"
#include "render_helpers.h"
#include "ui.h"

typedef enum player_move_mode_t
{
    PLAYER_MOVE_NORMAL,
    PLAYER_MOVE_NOCLIP,
} player_move_mode_t;

typedef struct player_t
{
    camera_t *attached_camera;

    player_move_mode_t move_mode;
    struct map_brush_t *support;

    v3_t p;
    v3_t dp;

    bool crouched;
    float crouch_t;
} player_t;

fn v3_t player_view_origin(player_t *player);
fn v3_t player_view_direction(player_t *player);
fn void player_noclip(player_t *player, float dt);
fn void init_view_for_camera(camera_t *camera, rect2_t viewport, r_view_t *view);

// frown
fn bool g_cursor_locked;

#include "world.h"

static world_t *g_world;
