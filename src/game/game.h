// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

fn void delay_next_frame(float milliseconds);

#include "action.h"
#include "asset.h"
#include "audio.h"
#include "bvh.h"
#include "camera.h"
#include "collision_geometry.h"
#include "dream.h"
#include "font.h"
#include "freeverb.h"
#include "entities.h"
#include "globals.h"
#include "input.h"
#include "intersect.h"
#include "job_queues.h"
#include "light_baker.h"
#include "log.h"
#include "map.h"
#include "mesh.h"
#include "physics_playground.h"
#include "ui.h"
#include "ui_layout.h"
#include "ui_widgets.h"
#include "ui_row_builder.h"
//#include "in_game_editor.h"

#include "render/r1.h" // @IncludeOrder

// editor stuff gets included after
#include "editor.h"
#include "editor_console.h"

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
fn void get_camera_matrices(camera_t *camera, rect2_t viewport, m4x4_t *view, m4x4_t *proj);

// frown
fn bool g_cursor_locked;

#include "world.h"

//static gamestate_t *g_game;
