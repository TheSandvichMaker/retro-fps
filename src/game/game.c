// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

// Tags:
// @UIInput
// @ActionSystem
// @ThreadSafety / @threadsafe (TODO: merge)
// @Globals
// @LoggingInCore
// @must-initialize (TODO: Deprecate?)
// @IncludeOrder

#include "game.h"

#include "action.c"
#include "asset.c"
#include "audio.c"
#include "bvh.c"
#include "camera.c"
#include "collision_geometry.c"
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
#include "ui_layout.c"
#include "editor_console.c"

#include "render/r1.c"

typedef struct app_state_t
{
	arena_t arena;

	double accumulator;

	gamestate_t     *game;
	action_system_t *action_system;
	console_t       *console;
	ui_t            *ui;

	bitmap_font_t   debug_font;
} app_state_t;

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

CVAR_F32(cvar_player_sprint_multiplier, "player.sprint_multiplier", 2.0f);
CVAR_F32(cvar_player_jump_force,        "player.jump_force",        300.0f);
CVAR_F32(cvar_player_crouch_speed,      "player.crouch_speed",      0.15f);

fn_local void register_player_cvars(void)
{
	cvar_register(&cvar_player_sprint_multiplier);
	cvar_register(&cvar_player_jump_force);
	cvar_register(&cvar_player_crouch_speed);
}

void player_noclip(player_t *player, float dt)
{
    camera_t *camera = player->attached_camera;

    if (!camera)
        return;

    player->support = NULL;

    float move_speed = 200.0f;
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

    if (action_held(Action_run))      move_speed *= 2.0f;

    if (action_held(Action_forward))  move_delta.x += 1.0f;
    if (action_held(Action_back))     move_delta.x -= 1.0f;
    if (action_held(Action_left))     move_delta.y += 1.0f;
    if (action_held(Action_right))    move_delta.y -= 1.0f;

    player->crouched = action_held(Action_crouch);

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
    
    if (player->support && action_pressed(Action_jump))
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

//
//
//

global action_system_t g_game_action_system = {0};

void app_init(platform_init_io_t *io)
{
	register_player_cvars();

	app_state_t *app = m_bootstrap(app_state_t, arena);
	io->app_state = app;

	app->ui = m_alloc_struct(&app->arena, ui_t);

	// @Globals - this is stupid
	equip_ui(app->ui);
	ui_initialize();
	unequip_ui();

	app->action_system = m_alloc_struct(&app->arena, action_system_t);
	equip_action_system(app->action_system);

	app->console = m_alloc_struct(&app->arena, console_t);
	// @UiFonts - this is stupid
	app->console->font = app->ui->style.font;

	bind_key_action(Action_left,           Key_a);
	bind_key_action(Action_right,          Key_d);
	bind_key_action(Action_forward,        Key_w);
	bind_key_action(Action_back,           Key_s);
	bind_key_action(Action_jump,           Key_space);
	bind_key_action(Action_run,            Key_shift);
	bind_key_action(Action_crouch,         Key_control);
	bind_mouse_button_action(Action_fire1, Button_left);
	bind_mouse_button_action(Action_fire2, Button_right);
	bind_key_action(Action_escape,         Key_escape);
	bind_key_action(Action_toggle_noclip,  Key_v);

	init_game_job_queues();

	initialize_asset_system(&(asset_config_t){
        .mix_sample_rate = DREAM_MIX_SAMPLE_RATE,
    });

	// pack_assets(S("../sources"));

    {
		asset_image_t *font_image = get_image_from_string(S("gamedata/textures/font.png"));
        app->debug_font.w = font_image->w;
        app->debug_font.h = font_image->h;
        app->debug_font.cw = 10;
        app->debug_font.ch = 12;
    }

	gamestate_t *game = app->game = m_bootstrap(gamestate_t, arena);
	equip_gamestate(game);

    game->fade_t = 1.0f;

	game->global_camera.vfov = 60.0f;
    compute_camera_axes(&game->global_camera);

	string_t startup_map = S("test");

    map_t    *map    = game->map    = load_map(&game->arena, Sf("gamedata/maps/%.*s.map", Sx(startup_map)));
    player_t *player = game->player = m_alloc_struct(&game->arena, player_t);

	game->primary_camera = &game->global_camera;
    player->attached_camera = game->primary_camera;

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

	unequip_action_system();
	unequip_gamestate();
}

fn_local void tick_game(gamestate_t *game, float dt)
{
	map_t    *map    = game->map;
	player_t *player = game->player;
	camera_t *camera = player->attached_camera;

	update_camera_rotation(camera, dt);

	if (action_pressed(Action_toggle_noclip))
	{
		if (player->move_mode == PLAYER_MOVE_NORMAL)
		{
			player->move_mode = PLAYER_MOVE_NOCLIP;
		}
		else
		{
			player->move_mode = PLAYER_MOVE_NORMAL;
		}
	}

	switch (player->move_mode)
	{
		case PLAYER_MOVE_NORMAL: player_movement(map, player, dt); break;
		case PLAYER_MOVE_NOCLIP: player_noclip  (player, dt);      break;
	}

	mixer_set_listener(camera->p, negate(camera->computed_z));
}

fn_local platform_cursor_t tick_ui(app_state_t *app, input_t *input, v2_t client_size, float dt)
{
	(void)dt;

	equip_ui(app->ui);

	// TODO: Is it cool or stupid that we're just translating between events
	// for "no" reason?
	// Cool:   decouple UI from platform abstraction
	// Uncool: cringe duplicate work
	for (platform_event_t *event = input->first_event;
		 event;
		 event = event->next)
	{
		switch (event->kind)
		{
			case Event_mouse_move:
			{
				ui_push_input_event(&(ui_event_t){
					.kind    = UiEvent_mouse_move,
					.mouse_p = event->mouse_move.mouse_p,
					.ctrl    = event->ctrl,
					.alt     = event->alt,
					.shift   = event->shift,
				});
			} break;

			case Event_mouse_wheel:
			{
				ui_push_input_event(&(ui_event_t){
					.kind        = UiEvent_mouse_wheel,
					.mouse_wheel = event->mouse_wheel.wheel,
					.mouse_p     = event->mouse_wheel.mouse_p,
					.ctrl        = event->ctrl,
					.alt         = event->alt,
					.shift       = event->shift,
				});
			} break;

			case Event_mouse_button:
			{
				ui_buttons_t button = 0;

				switch (event->mouse_button.button)
				{
					case Button_left:   button = UiButton_left;   break;
					case Button_middle: button = UiButton_middle; break;
					case Button_right:  button = UiButton_right;  break;
					case Button_x1:     button = UiButton_x1;     break;
					case Button_x2:     button = UiButton_x2;     break;
					INVALID_DEFAULT_CASE;
				}

				ui_push_input_event(&(ui_event_t){
					.kind    = UiEvent_mouse_button,
					.mouse_p = event->mouse_button.mouse_p,
					.pressed = event->mouse_button.pressed,
					.button  = button,
					.ctrl    = event->ctrl,
					.alt     = event->alt,
					.shift   = event->shift,
				});
			} break;

			case Event_key:
			{
				ui_push_input_event(&(ui_event_t){
					.kind    = UiEvent_key,
					.keycode = event->key.keycode,
					.pressed = event->key.pressed,
					.ctrl    = event->ctrl,
					.alt     = event->alt,
					.shift   = event->shift,
				});
			} break;

			case Event_text:
			{
				ui_event_t ui_event = { 
					.kind    = UiEvent_text,
					.ctrl    = event->ctrl,
					.alt     = event->alt,
					.shift   = event->shift,
				};
				string_into_storage(ui_event.text, string_from_storage(event->text.text));

				ui_push_input_event(&ui_event);
			} break;
		}
	}

	equip_gamestate(app->game);

	ui_begin(dt);
	{
		update_and_render_in_game_editor();
		(void)client_size;
		//update_and_draw_console(app->console, client_size, dt);
	}
    ui_end();

	unequip_gamestate();

	// @Globals
	unequip_ui();

	return app->ui->cursor;
}

fn_local void render_game(/*r1_t *r1, */gamestate_t *game, rhi_window_t window)
{
	// @Globals
	// equip_r1(r1);

	rhi_command_list_t *list = rhi_get_command_list();
	rhi_texture_t backbuffer = rhi_get_current_backbuffer(window);

	map_t *map = game->map;

	if (map)
	{
		player_t *player = game->player;
		camera_t *camera = player->attached_camera;

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

		// scene->skybox = skybox;
		scene->sun_direction   = normalize(make_v3(0.25f, 0.75f, 1));
		scene->sun_color       = sun_color;

		scene->fogmap          = map->fogmap;
		scene->fog_offset      = rect3_center(map->bounds);
		scene->fog_dim         = rect3_dim(map->bounds);
		scene->fog_absorption  = 0.002f;
		scene->fog_density     = 0.02f;
		scene->fog_scattering  = 0.04f;
		scene->fog_phase_k     = 0.6f;

		r1_update_window_resources(window);
		r1_render_game_view(list, backbuffer, &view, map);
	}

	// unequip_r1();
}

fn_local void app_tick(platform_tick_io_t *io)
{
	app_state_t     *app            = io->app_state;
	gamestate_t     *game           = app->game;
	action_system_t *action_system  = app->action_system;
	ui_t            *the_ui         = app->ui;
	//r1_state_t       *r1             = app->r1;

	equip_action_system(action_system);
	ingest_action_system_input(io->input);

	suppress_actions(!io->has_focus || the_ui->has_focus);

	const double sim_rate = 240.0;
	const double dt       = 1.0 / sim_rate;

	// clamp frame time to avoid death spirals
	double frame_time = MIN(io->frame_time, 0.1);

	app->accumulator += frame_time;

	// why are you inconsisteeeeent
	if (action_pressed(Action_fire2))
	{
		the_ui->has_focus = true;
	}

	bool first_iteration = true;
	while (app->accumulator >= dt)
	{
		tick_game(game, (float)dt);
		app->accumulator -= dt;

		if (first_iteration)
		{
			action_system_clear_sticky_edges();
			first_iteration = false;
		}
	}

	unequip_action_system();

	rhi_window_t window = io->rhi_window;
	v2_t client_size = rhi_get_window_client_size(window);

	io->cursor = tick_ui(app, io->input, client_size, (float)frame_time);

	rhi_begin_frame();

	render_game(game, window);
	r1_render_ui(rhi_get_command_list(), rhi_get_current_backbuffer(window), &the_ui->render_commands);

	rhi_end_frame();

	process_asset_changes();

	io->cursor_locked = !the_ui->has_focus;
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

static void app_mix_audio(platform_audio_io_t *io)
{
	mix_samples((uint32_t)io->frames_to_mix, io->buffer);
}

void platform_init(size_t argc, string_t *argv, platform_hooks_t *hooks)
{
	(void)argc;
	(void)argv;

	hooks->init       = app_init;
	hooks->tick       = app_tick;
	hooks->tick_audio = app_mix_audio;
}
