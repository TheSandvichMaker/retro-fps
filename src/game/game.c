// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#include "game.h"

#include "asset.c"
#include "audio.c"
#include "bvh.c"
#include "camera.c"
#include "collision_geometry.c"
#include "convar.c"
#include "convex_hull_debugger.c"
#include "font.c"
#include "freeverb.c"
#include "in_game_editor.c"
#include "input.c"
#include "intersect.c"
#include "job_queues.c"
#include "light_baker.c"
#include "log.c"
#include "map.c"
#include "mesh.c"
#include "physics_playground.c"
#include "render.c"
#include "render_backend.c"
#include "render_helpers.c"
#include "ui.c"

#include "render/r1.c"

#define UI_FORCE_ONE_DRAW_CALL_PER_RECT_SLOW_DEBUG_STUFF 0

#if 0
    OK here's an issue: rc->screenspace is still being used. It shouldn't be.
    UI for the game should be in the UI layer of the scene view, not a separate screenspace
    view. So that needs to be solved
#endif

static bool initialized = false;

bool g_cursor_locked;

static texture_handle_t skybox;
static bitmap_font_t debug_font;
static waveform_t *test_waveform;
static waveform_t *short_sound;

static mixer_id_t music;

static triangle_mesh_t test_convex_hull;

v3_t player_view_origin(player_t *player)
{
    v3_t result = player->p;
    float player_height = lerp(56.0f, 32.0f, smootherstep(player->crouch_t));
    result.z += player_height;
    return result;
}

v3_t player_view_direction(player_t *player)
{
    camera_t *camera = player->attached_camera;

    if (!camera)
        return make_v3(1, 0, 0);
    
    rotor3_t r_pitch = rotor3_from_plane_angle(PLANE_ZX, DEG_TO_RAD*camera->pitch);
    rotor3_t r_yaw   = rotor3_from_plane_angle(PLANE_YZ, DEG_TO_RAD*camera->yaw);

    v3_t dir = { 1, 0, 0 };
    dir = rotor3_rotatev(r_pitch, dir);
    dir = rotor3_rotatev(r_yaw,   dir);

    return dir;
}

void player_noclip(player_t *player, float dt)
{
    camera_t *camera = player->attached_camera;

    if (!camera)
        return;

    player->support = NULL;

    float move_speed = 200.0f;
    v3_t move_delta = { 0 };

    if (button_down(BUTTON_RUN))      move_speed *= 2.0f;

    if (button_down(BUTTON_FORWARD))  move_delta.x += move_speed;
    if (button_down(BUTTON_BACK))     move_delta.x -= move_speed;
    if (button_down(BUTTON_LEFT))     move_delta.y += move_speed;
    if (button_down(BUTTON_RIGHT))    move_delta.y -= move_speed;

    rotor3_t r_pitch = rotor3_from_plane_angle(PLANE_ZX, DEG_TO_RAD*camera->pitch);
    rotor3_t r_yaw   = rotor3_from_plane_angle(PLANE_YZ, DEG_TO_RAD*camera->yaw  );

    move_delta = rotor3_rotatev(r_pitch, move_delta);
    move_delta = rotor3_rotatev(r_yaw  , move_delta);

    if (button_down(BUTTON_JUMP))     move_delta.z += move_speed;
    if (button_down(BUTTON_CROUCH))   move_delta.z -= move_speed;

    player->dp = move_delta;
    player->p  = add(player->p, mul(dt, player->dp));

    camera->p = player_view_origin(player);
}

void player_movement(map_t *map, player_t *player, float dt)
{
    camera_t *camera = player->attached_camera;

    if (!camera)
        return;

    // quat_t qyaw = make_quat_axis_angle((v3_t){ 0, 0, 1 }, DEG_TO_RAD*camera->yaw);
    rotor3_t r_yaw = rotor3_from_plane_angle(PLANE_YZ, DEG_TO_RAD*camera->yaw);

    // movement

    float move_speed = 1500.0f;
    v3_t move_delta = { 0 };

    if (button_down(BUTTON_RUN))      move_speed *= 2.0f;

    if (button_down(BUTTON_FORWARD))  move_delta.x += 1.0f;
    if (button_down(BUTTON_BACK))     move_delta.x -= 1.0f;
    if (button_down(BUTTON_LEFT))     move_delta.y += 1.0f;
    if (button_down(BUTTON_RIGHT))    move_delta.y -= 1.0f;

    player->crouched = button_down(BUTTON_CROUCH);

    float max_player_height = 56.0f;

    rect3_t max_player_bounds = {
        .min = { -16, -16, -24 },
        .max = {  16,  16, 0 },
    };

    for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
    {
        map_brush_t *brush = &map->brushes[brush_index];

        if (brush == player->support)
            continue;

        rect3_t bounds = rect3_add(max_player_bounds, brush->bounds);

        v3_t r_o = player->p;
        r_o.z += 0.01f;

        float hit_t = ray_intersect_rect3(r_o, (v3_t){0, 0, 1}, bounds);
        if (hit_t > 0.001f && hit_t < max_player_height)
        {
            max_player_height = hit_t;
        }
    }

    float crouch_animation_speed = 0.15f;
    if (player->crouched)
    {
        if (player->crouch_t < 1.0f)
        {
            player->crouch_t += dt/crouch_animation_speed;

            if (player->crouch_t > 1.0f)
                player->crouch_t = 1.0f;
        }
    }
    else
    {
        float min_crouch_t = max(0.0f, 1.0f - max_player_height / 56.0f);
        if (player->crouch_t > min_crouch_t)
        {
            player->crouch_t -= dt/crouch_animation_speed;

            if (player->crouch_t < min_crouch_t)
                player->crouch_t = min_crouch_t;
        }
    }

    float player_height = lerp(56.0f, 32.0f, smootherstep(player->crouch_t));

    rect3_t player_bounds = {
        .min = { -16, -16, -player_height },
        .max = {  16,  16,  0  },
    };

    move_delta = rotor3_rotatev(r_yaw, move_delta);
    move_delta = normalize_or_zero(move_delta);
    move_delta = mul(move_delta, move_speed);

    // physics
    
    if (player->support && button_pressed(BUTTON_JUMP))
    {
        float jump_force = 300.0f;
        player->support = NULL;
        player->dp.z += jump_force;
    }

    if (player->support)
    {
        rect3_t bounds = rect3_add(player_bounds, player->support->bounds);
        if (player->p.x > bounds.max.x ||
            player->p.y > bounds.max.y ||
            player->p.x < bounds.min.x ||
            player->p.y < bounds.min.y)
        {
            player->support = NULL;
        }
    }

    if (!player->support)
    {
        // air controls
        float c = dot(normalize_or_zero(move_delta), normalize_or_zero(player->dp));
        c = max(0.0f, 1.0f - c);
        move_delta = mul(move_delta, c);
        move_delta = mul(move_delta, 0.25f);
    }

    v3_t ddp = move_delta;

    if (player->support)
    {
        v3_t friction = mul(10.0f, negate(player->dp));
        friction.z = 0.0f;
        ddp = add(ddp, friction);
    }

    if (!player->support)
    {
        v3_t gravity = { 0, 0, -8.192f };
        ddp = add(ddp, mul(100.0f, gravity));
    }

    player->dp = add(player->dp, mul(dt, ddp));

    v3_t dp = mul(dt, player->dp);
    float dp_len = vlen(dp);

    float t_min  = 0.001f;
    float t_max  = dp_len;
    float move_t = t_max;

    // collide player against map geometry

    for (size_t i = 0; i < 4; i++)
    {
        if (move_t <= 0.01f)
            break;

        float time_spent = move_t / t_max;

        dp = mul(time_spent*dt, player->dp);
        dp_len = vlen(dp);

        v3_t r_o = player->p;
        v3_t r_d = normalize_or_zero(dp);

        float closest_hit_t = dp_len;
        map_brush_t *closest_hit_brush = NULL;

        for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
        {
            map_brush_t *brush = &map->brushes[brush_index];

            rect3_t bounds = rect3_add(player_bounds, brush->bounds);

            float hit_t = ray_intersect_rect3(r_o, r_d, bounds);
            if (hit_t > t_min && hit_t <= closest_hit_t)
            {
                if (hit_t < closest_hit_t)
                {
                    closest_hit_brush = brush;
                    closest_hit_t = hit_t;
                }
            }
        }

        if (closest_hit_brush)
        {
            map_brush_t *hit_brush = closest_hit_brush;

            rect3_t bounds = rect3_add(player_bounds, hit_brush->bounds);

            v3_t hit_p = add(r_o, mul(r_d, closest_hit_t));
            v3_t n = get_normal_rect3(hit_p, bounds);

            if (n.z > 0.25f)
            {
                player->support = hit_brush;
                player->dp.z = 0.0f;
            }

            v3_t straightening_force = mul(n, dot(player->dp, n));
            player->dp = sub(player->dp, straightening_force);
        }

        float t = max(0.0f, min(move_t, closest_hit_t - 0.1f));

        v3_t final_move = add(player->p, mul(r_d, t));

        player->p = final_move;
        move_t -= t;

#if 0
        diag_add_arrow(&(diag_arrow_t){
            .start = r_o,
            .end   = add(r_o, mul(t, r_d)),
            .color = { 1, 1, 0 },
        });
#endif
    }

#if 0
    diag_add_arrow(&(diag_arrow_t){
        .start = player->p,
        .end   = add(player->p, v3(1, 0, 0)),
        .color = { 1, 0, 0 },
    });
    diag_add_arrow(&(diag_arrow_t){
        .start = player->p,
        .end   = add(player->p, v3(0, 1, 0)),
        .color = { 0, 1, 0 },
    });
    diag_add_arrow(&(diag_arrow_t){
        .start = player->p,
        .end   = add(player->p, v3(0, 0, 1)),
        .color = { 0, 0, 1 },
    });
#endif

    camera->p = player_view_origin(player);
}

static camera_t g_camera = {
    .vfov = 60.0f,
};

fn void init_view_for_camera(camera_t *camera, rect2_t viewport, r_view_t *view)
{
    view->clip_rect = viewport;
    view->camera_p  = camera->p;

    v3_t p = camera->p;
    v3_t d = negate(camera->computed_z);

    v3_t up = { 0, 0, 1 };
    view->view_matrix = make_view_matrix(p, d, up);

    float w = viewport.max.x - viewport.min.x;
    float h = viewport.max.y - viewport.min.y;

    float aspect = w / h;
    view->proj_matrix = make_perspective_matrix(camera->vfov, aspect, 1.0f);
}

void game_init(platform_init_io_t *io)
{
	(void)io;

	init_game_job_queues();

	initialize_asset_system(&(asset_config_t){
        .mix_sample_rate = DREAM_MIX_SAMPLE_RATE,
    });

	// pack_assets(S("../sources"));

    update_camera_rotation(&g_camera, 0.0f);

    {
		asset_image_t *font_image = get_image_from_string(S("gamedata/textures/font.png"));
        debug_font.w = font_image->w;
        debug_font.h = font_image->h;
        debug_font.cw = 10;
        debug_font.ch = 12;
        //debug_font.texture = font_image->renderer_handle;
    }

	gamestate_t *game = g_game = m_bootstrap(gamestate_t, arena);
    game->fade_t = 1.0f;

	string_t startup_map = S("test");

    map_t    *map    = game->map    = load_map(&game->arena, Sf("gamedata/maps/%.*s.map", Sx(startup_map)));
    player_t *player = game->player = m_alloc_struct(&game->arena, player_t);

	if (!map)
	{
		FATAL_ERROR("Failed to load map %.*s", Sx(startup_map));
	}

	for (size_t entity_index = 0; entity_index < map->entity_count; entity_index++)
	{
		map_entity_t *e = &map->entities[entity_index];

		if (is_class(map, e, S("worldspawn")))
		{
			if (game->worldspawn)
			{
				log(MapLoad, Warning, "More than one worldspawn in the map. Overwriting previously seen one!");
			}

			game->worldspawn = e;
		}

		if (is_class(map, e, S("info_player_start")))
		{
			player->p = v3_from_key(map, e, S("origin"));
		}
	}

    map->fog_absorption  = 0.002f;
    map->fog_density     = 0.02f;
    map->fog_scattering  = 0.04f;
    map->fog_phase_k     = 0.6f;

	//
	// R1
	//

	r1_init();
	r1_init_map_resources(map);

	//
	//
	//

    initialized = true;
}

fn_local void game_update(platform_update_io_t *io)
{
	gamestate_t *game = g_game;
	ASSERT_MSG(game, "You need to call game_init before calling game_update");

	float dt = io->dt;

	// TODO: Totally redo input 
	{
		/*
		ui_submit_mouse_p    (io->input->mouse_p);
		ui_submit_mouse_dp   (io->input->mouse_dp);
		ui_submit_mouse_wheel(io->input->mouse_wheel);
		*/

		input_state_t input = {0};
		input.mouse_x  = (int)io->input->mouse_p.x;
		input.mouse_y  = (int)io->input->mouse_p.y;
		input.mouse_dx = (int)io->input->mouse_dp.x;
		input.mouse_dy = (int)io->input->mouse_dp.y;

		static uint64_t button_states = 0;

		for (platform_event_t *ev = platform_event_iter(io->input->first_event);
			 ev;
			 ev = platform_event_next(ev))
		{
			switch (ev->kind)
			{
				case Event_mouse_button:
				{
					bool pressed = ev->mouse_button.pressed;
					//ui_submit_mouse_buttons(ev->mouse_button.button, pressed);

					switch (ev->mouse_button.button)
					{
						case Button_left:
						{
							button_states = TOGGLE_BIT(button_states, BUTTON_FIRE1, pressed);
						} break;

						case Button_right:
						{
							button_states = TOGGLE_BIT(button_states, BUTTON_FIRE2, pressed);
						} break;
					}
				} break;

				case Event_key:
				{
					bool pressed = ev->key.pressed;

					switch ((int)ev->key.keycode) // int cast just stops MSVC from complaining that the ascii cases are not valid values of the enum (TODO: add them)
					{
						case 'W':         button_states = TOGGLE_BIT(button_states, BUTTON_FORWARD      , pressed); break;
						case 'A':         button_states = TOGGLE_BIT(button_states, BUTTON_LEFT         , pressed); break;
						case 'S':         button_states = TOGGLE_BIT(button_states, BUTTON_BACK         , pressed); break;
						case 'D':         button_states = TOGGLE_BIT(button_states, BUTTON_RIGHT        , pressed); break;
						case 'V':         button_states = TOGGLE_BIT(button_states, BUTTON_TOGGLE_NOCLIP, pressed); break;
						case Key_space:   button_states = TOGGLE_BIT(button_states, BUTTON_JUMP         , pressed); break;
						case Key_control: button_states = TOGGLE_BIT(button_states, BUTTON_CROUCH       , pressed); break;
						case Key_shift:   button_states = TOGGLE_BIT(button_states, BUTTON_RUN          , pressed); break;
						case Key_escape:  button_states = TOGGLE_BIT(button_states, BUTTON_ESCAPE       , pressed); break;
						case Key_f1:      button_states = TOGGLE_BIT(button_states, BUTTON_F1           , pressed); break;
						case Key_f2:      button_states = TOGGLE_BIT(button_states, BUTTON_F2           , pressed); break;
						case Key_f3:      button_states = TOGGLE_BIT(button_states, BUTTON_F3           , pressed); break;
						case Key_f4:      button_states = TOGGLE_BIT(button_states, BUTTON_F4           , pressed); break;
						case Key_f5:      button_states = TOGGLE_BIT(button_states, BUTTON_F5           , pressed); break;
						case Key_f6:      button_states = TOGGLE_BIT(button_states, BUTTON_F6           , pressed); break;
						case Key_f7:      button_states = TOGGLE_BIT(button_states, BUTTON_F7           , pressed); break;
						case Key_f8:      button_states = TOGGLE_BIT(button_states, BUTTON_F8           , pressed); break;
						case Key_f9:      button_states = TOGGLE_BIT(button_states, BUTTON_F9           , pressed); break;
						case Key_f10:     button_states = TOGGLE_BIT(button_states, BUTTON_F10          , pressed); break;
						case Key_f11:     button_states = TOGGLE_BIT(button_states, BUTTON_F11          , pressed); break;
						case Key_f12:     button_states = TOGGLE_BIT(button_states, BUTTON_F12          , pressed); break;
					}
				} break;

				case Event_text:
				{
					//string_t text = string_from_storage(ev->text.text);
					//ui_submit_text(text);
				} break;
			}
		}

		input.button_states = button_states;

		update_input_state(&input);
	}

	//ui.input.app_has_focus = io->has_focus;
	//io->cursor = ui.input.cursor;

	// bool ui_focused = ui_begin(dt);

    // if (ui_focused)
    // {
    //     suppress_game_input(true);
    // }

    if (button_pressed(BUTTON_FIRE2))
	{
        g_cursor_locked = !g_cursor_locked;
	}

    io->lock_cursor = g_cursor_locked;

    game->primary_camera = &g_camera;

    if (button_pressed(BUTTON_FIRE1))
	{
		static bool mono = true;

		mono = !mono;

		if (mono)
		{
			update_playing_sound_flags(music, 0, PLAY_SOUND_FORCE_MONO);
		}
		else
		{
			update_playing_sound_flags(music, PLAY_SOUND_FORCE_MONO, 0);
		}
	}

    map_t *map = game->map;

    player_t *player = game->player;
    player->attached_camera = &g_camera;

    camera_t *camera = player->attached_camera;

    if (button_pressed(BUTTON_TOGGLE_NOCLIP))
    {
        if (player->move_mode == PLAYER_MOVE_NORMAL)
            player->move_mode = PLAYER_MOVE_NOCLIP;
        else
            player->move_mode = PLAYER_MOVE_NORMAL;
    }

    if (io->lock_cursor)
        update_camera_rotation(camera, dt);

    switch (player->move_mode)
    {
        case PLAYER_MOVE_NORMAL:  player_movement(map, player, dt); break;
        case PLAYER_MOVE_NOCLIP: player_noclip(player, dt); break;
    }

	mixer_set_listener(camera->p, negate(camera->computed_z));

    game->fade_t += 0.45f*dt*(game->fade_target_t - game->fade_t);

    //update_and_render_in_game_editor();

    //ui_end();

	//
	//
	//

    if (button_pressed(BUTTON_ESCAPE))
    {
        io->request_exit = true;
    }
}

#if 0
typedef struct transform_t
{
	v3_t   translation;
	quat_t rotation;
	v3_t   scale;
} transform_t;

typedef struct render_entity_t
{
	struct render_entity_t *next;

	transform_t transform;

	// I don't care what's in here right now 
	int dummy;
} render_entity_t;

typedef struct debug_line_block_t
{
	struct debug_line_block_t *next;
	struct debug_line_block_t *prev;

	uint32_t     simulation_step_index;
	uint32_t     count;
	debug_line_t lines[512];
} debug_line_block_t;

typedef struct render_frame_t
{
	arena_t *arena;

	// per-frame render state:
	debug_line_block_t *head_debug_line_block;
	debug_line_block_t *tail_debug_line_block;

	// static render state:
	map_t *map;
} render_frame_t;

typedef struct render_world_t
{
	// sim render state (can be interpolated):
} render_world_t;
#endif

fn_local void game_update_ui(platform_update_ui_io_t *io)
{
	input_t *input = io->input;

	ui.app_has_focus = io->has_focus;

	bool ui_focused = ui_begin(input, io->dt);
	(void)ui_focused;

    update_and_render_in_game_editor();

    ui_end();

	io->cursor = ui.cursor;
}

fn_local void game_render(platform_render_io_t *io)
{
	process_asset_changes();

	if (!r1)
	{
		return;
	}

	gamestate_t *game = g_game;

	if (game)
	{
		map_t *map = game->map;

		if (map)
		{
			player_t *player = game->player;
			camera_t *camera = player->attached_camera;

			rhi_texture_t backbuffer = rhi_get_current_backbuffer(io->rhi_window);

			const rhi_texture_desc_t *desc = rhi_get_texture_desc(backbuffer);

			rect2_t viewport = {
				.min = { 0.0f, 0.0f },
				.max = { (float)desc->width, (float)desc->height },
			};

			r_view_t view = {0};
			init_view_for_camera(camera, viewport, &view);

			r_scene_parameters_t *scene = &view.scene;

			map_entity_t *worldspawn = game->worldspawn;

			float sun_brightness = float_from_key(map, worldspawn, S("sun_brightness"));
			v3_t  sun_color      = v3_normalize(v3_from_key(map, worldspawn, S("sun_color")));
				  sun_color      = mul(sun_brightness, sun_color);

			scene->skybox = skybox;
			scene->sun_direction   = normalize(make_v3(0.25f, 0.75f, 1));
			scene->sun_color       = sun_color;

			scene->fogmap          = map->fogmap;
			scene->fog_offset      = rect3_center(map->bounds);
			scene->fog_dim         = rect3_dim(map->bounds);
			scene->fog_absorption  = 0.002f;
			scene->fog_density     = 0.02f;
			scene->fog_scattering  = 0.04f;
			scene->fog_phase_k     = 0.6f;

			r1_update_window_resources(io->rhi_window);
			r1_render_game_view(io->rhi_command_list, rhi_get_current_backbuffer(io->rhi_window), &view, map);
		}
	}

	ui_render_command_list_t *ui_commands = ui_get_render_commands();
	r1_render_ui(io->rhi_command_list, rhi_get_current_backbuffer(io->rhi_window), ui_commands);
}

static void game_mix_audio(size_t frame_count, float *frames)
{
	mix_samples((uint32_t)frame_count, frames);
}

void platform_init(size_t argc, string_t *argv, platform_hooks_t *hooks)
{
	(void)argc;
	(void)argv;

	hooks->init       = game_init;
	hooks->update     = game_update;
	hooks->update_ui  = game_update_ui;
	hooks->render     = game_render;
	hooks->tick_audio = game_mix_audio;
}
