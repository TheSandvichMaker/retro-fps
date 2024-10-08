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

static float g_next_frame_delay_ms = 0.0f;

#include "game.h"

#include "action.c"
#include "asset.c"
#include "audio.c"
#include "bvh.c"
#include "camera.c"
#include "collision_geometry.c"
#include "editor.c"
#include "editor_console.c"
#include "entities.c"
#include "font.c"
#include "freeverb.c"
#include "input.c"
#include "intersect.c"
#include "job_queues.c"
#include "light_baker.c"
#include "log.c"
#include "map.c"
#include "mesh.c"
#include "physics_playground.c"
#include "ui.c"
#include "ui_row_builder.c"
#include "ui_widgets.c"
//#include "in_game_editor.c"

#include "render/r1.c"

void delay_next_frame(float milliseconds)
{
	g_next_frame_delay_ms = milliseconds;
}

typedef struct app_state_t
{
	arena_t arena;

	double accumulator;

	gamestate_t     *game;
	action_system_t *action_system;
	editor_t        *editor;
	console_t       *console;
	ui_t            *ui;
	rhi_state_t     *rhi_state;
	r1_state_t      *r1;
	cvar_state_t     cvar_state;
	asset_system_t  *assets;

	rhi_window_t rhi_window;
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

fn_local void teleport_player(player_t *player, v3_t p)
{
	player->p       = p;
	player->support = NULL;
}

CVAR_F32(cvar_player_sprint_multiplier, "player.sprint_multiplier", 2.0f);
CVAR_F32(cvar_player_jump_force,        "player.jump_force",        300.0f);
CVAR_F32(cvar_player_crouch_speed,      "player.crouch_speed",      0.15f);

CVAR_COMMAND(ccmd_respawn_player, "player.respawn")
{
	(void)arguments;

	if (!g_game)
	{
		return;
	}

	gamestate_t *game = g_game;

	map_t    *map    = game->map;
	player_t *player = game->player;

	for (size_t entity_index = 0; entity_index < map->entity_count; entity_index++)
	{
		map_entity_t *e = &map->entities[entity_index];

		if (is_class(map, e, S("info_player_start")))
		{
			v3_t p = v3_from_key(map, e, S("origin"));

			teleport_player(player, p);
			player->dp = v3s(0.0f);

			break;
		}
	}
}

fn_local void register_player_cvars(void)
{
	cvar_register(&cvar_player_sprint_multiplier);
	cvar_register(&cvar_player_jump_force);
	cvar_register(&cvar_player_crouch_speed);
	cvar_register(&ccmd_respawn_player);
}

void player_noclip(player_t *player, float dt)
{
    camera_t *camera = player->attached_camera;

    if (!camera)
        return;

    player->support = NULL;

    float move_speed = 200.0f;
    v3_t move_delta = { 0 };

    if (action_held(Action_run))      move_speed *= cvar_read_f32(&cvar_player_sprint_multiplier);

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

    if (action_held(Action_run))      move_speed *= cvar_read_f32(&cvar_player_sprint_multiplier);

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

    float crouch_animation_speed = cvar_read_f32(&cvar_player_crouch_speed);
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
        float jump_force = cvar_read_f32(&cvar_player_jump_force);
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
    }

    camera->p = player_view_origin(player);
}

void get_camera_matrices(camera_t *camera, rect2_t viewport, m4x4_t *view, m4x4_t *proj)
{
    v3_t p = camera->p;
    v3_t d = negate(camera->computed_z);

    v3_t up = { 0, 0, 1 };
	*view = make_view_matrix(p, d, up);

    float w = viewport.max.x - viewport.min.x;
    float h = viewport.max.y - viewport.min.y;

    float aspect = w / h;
	*proj = make_perspective_matrix(camera->vfov, aspect, 1.0f);
}

//
//
//

global action_system_t g_game_action_system = {0};

void app_init(platform_init_io_t *io)
{
	app_state_t *app = m_bootstrap(app_state_t, arena);
	io->app_state = app;

	cvar_state_init(&app->cvar_state);
	equip_cvar_state(&app->cvar_state);

	app->rhi_state = rhi_init(&(rhi_init_params_t){
		.frame_latency = 2,
	});

	rhi_equip_state(app->rhi_state);

	app->rhi_window = rhi_init_window(io->os_window_handle);

	register_player_cvars();

	app->ui = m_alloc_struct(&app->arena, ui_t);

	equip_ui(app->ui);
	ui_initialize();
	unequip_ui();

	app->action_system = m_alloc_struct(&app->arena, action_system_t);
	equip_action_system(app->action_system);

	app->console = m_alloc_struct(&app->arena, console_t);

	app->editor = m_alloc_struct(&app->arena, editor_t);
	editor_init(app->editor);

	bind_key_action         (Action_left,           Key_a);
	bind_key_action         (Action_right,          Key_d);
	bind_key_action         (Action_forward,        Key_w);
	bind_key_action         (Action_back,           Key_s);
	bind_key_action         (Action_look_left,      Key_left);
	bind_key_action         (Action_look_right,     Key_right);
	bind_key_action         (Action_look_up,        Key_down);
	bind_key_action         (Action_look_down,      Key_up);
	bind_key_action         (Action_look_faster,    Key_f);
	bind_key_action         (Action_jump,           Key_space);
	bind_key_action         (Action_run,            Key_shift);
	bind_key_action         (Action_crouch,         Key_control);
	bind_mouse_button_action(Action_fire1,          Button_left);
	bind_mouse_button_action(Action_fire2,          Button_right);
	bind_key_action         (Action_escape,         Key_escape);
	bind_key_action         (Action_toggle_noclip,  Key_v);

	bind_key_console_command(Key_backspace, &ccmd_respawn_player);

	init_game_job_queues();

	app->assets = asset_system_make();

	asset_system_equip(app->assets);

	gamestate_t *game = app->game = m_bootstrap(gamestate_t, arena);
	equip_gamestate(game);

    game->fade_t = 1.0f;

	game->global_camera.vfov = 60.0f;
    compute_camera_axes(&game->global_camera);

	string_t startup_map = S("test");

    map_t    *map    = game->map    = load_map(&game->arena, Sf("gamedata/maps/%cs.map", startup_map));
    player_t *player = game->player = m_alloc_struct(&game->arena, player_t);

	game->primary_camera = &game->global_camera;
    player->attached_camera = game->primary_camera;

	if (!map)
	{
		FATAL_ERROR("Failed to load map %cs", startup_map);
	}

	for (size_t entity_index = 0; entity_index < map->entity_count; entity_index++)
	{
		map_entity_t *e = &map->entities[entity_index];

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

	app->r1 = r1_make();

	r1_equip(app->r1);
	r1_init_map_resources(map);
	r1_unequip();

	//
	//
	//

	unequip_action_system();
	unequip_gamestate();
	asset_system_unequip();
	rhi_unequip_state();
	unequip_cvar_state();
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

fn_local void tick_ui(platform_tick_io_t *io, app_state_t *app, input_t *input, v2_t client_size, float dt)
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
				ui_queue_event(&(ui_event_t){
					.kind    = UiEvent_mouse_move,
					.mouse_p = event->mouse_move.mouse_p,
					.ctrl    = event->ctrl,
					.alt     = event->alt,
					.shift   = event->shift,
				});
			} break;

			case Event_mouse_wheel:
			{
				ui_queue_event(&(ui_event_t){
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

				ui_queue_event(&(ui_event_t){
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
				ui_queue_event(&(ui_event_t){
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

				ui_queue_event(&ui_event);
			} break;
		}
	}

	equip_gamestate(app->game);

	ui_begin(dt, rect2_from_min_dim(make_v2(0, 0), client_size));
	{
		editor_update_and_render(app->editor);
		// update_and_draw_console(app->console, client_size, dt);
	}
    ui_end();

	unequip_gamestate();

	io->restrict_mouse_rect = ui->restrict_mouse_rect;
	io->cursor              = ui->cursor;
	io->set_mouse_p         = ui->set_mouse_p;

	unequip_ui();
}

fn_local void render_game(gamestate_t *game, rhi_window_t window, ui_render_command_list_t *ui_list)
{
	PROFILE_FUNC_BEGIN;

	// @Globals
	// equip_r1(r1);

	rhi_command_list_t *list = rhi_get_command_list();
	rhi_texture_t backbuffer = rhi_get_current_backbuffer(window);

	map_t *map = game->map;

	if (map)
	{
		player_t *player = game->player;
		camera_t *camera = player->attached_camera;

		for (size_t light_index = 0; light_index < map->light_count; light_index++)
		{
			map_point_light_t *light = &map->lights[light_index];

			rect3_t rect = rect3_center_radius(light->p, v3_from_scalar(4.0f));
			draw_debug_cube(rect, COLORF_YELLOW);
		}

		const rhi_texture_desc_t *desc = rhi_get_texture_desc(backbuffer);

		rect2_t viewport = {
			.min = { 0.0f, 0.0f },
			.max = { (float)desc->width, (float)desc->height },
		};

		r1_view_t view = r1_create_view(window);
		view.camera_p  = camera->p;
		view.clip_rect = viewport;
		get_camera_matrices(camera, viewport, &view.view_matrix, &view.proj_matrix);

		r1_scene_parameters_t *scene = &view.scene;
		{
			worldspawn_t *worldspawn = map->worldspawn;

			float sun_brightness = worldspawn->sun_brightness;
			v3_t  sun_color      = worldspawn->sun_color;
			      sun_color      = mul(sun_brightness, sun_color);

			scene->sun_direction = normalize(make_v3(0.25f, 0.75f, 1));
			scene->sun_color     = sun_color;

			// scene->fogmap                   = map->fogmap;
			scene->fog_offset               = rect3_center(map->bounds);
			scene->fog_dim                  = rect3_dim(map->bounds);
			scene->fog_absorption           = worldspawn->fog_absorption;
			scene->fog_density              = worldspawn->fog_density;
			scene->fog_scattering           = worldspawn->fog_scattering;
			scene->fog_phase_k              = worldspawn->fog_phase;
			scene->fog_ambient_inscattering = worldspawn->fog_ambient_inscattering;
		}

		r1_render_game_view(list, &view, map);
		r1_render_ui(list, &view, ui_list);
	}

	PROFILE_FUNC_END;
}

fn_local void app_tick(platform_tick_io_t *io)
{
	app_state_t     *app            = io->app_state;
	gamestate_t     *game           = app->game;
	action_system_t *action_system  = app->action_system;
	ui_t            *the_ui         = app->ui;
	cvar_state_t    *cvar_state     = &app->cvar_state;

	equip_cvar_state(cvar_state);
	asset_system_equip(app->assets);

	equip_action_system(action_system);
	suppress_actions(!io->has_focus || the_ui->has_focus);

	cmd_execution_list_t cmd_list = ingest_action_system_input(m_get_temp(NULL, 0), io->input);

	const double sim_rate = 240.0;
	const double dt       = 1.0 / sim_rate;

	// clamp frame time to avoid death spirals
	double frame_time = MIN(io->frame_time, 0.1);

	if (g_next_frame_delay_ms != 0.0f)
	{
		os_sleep(g_next_frame_delay_ms);
		g_next_frame_delay_ms = 0.0f;
	}

	app->accumulator += frame_time;

	if (action_pressed(Action_fire2))
	{
		the_ui->has_focus = true;
	}

	equip_gamestate(app->game);

	for (cmd_execution_node_t *node = cmd_list.head; node; node = node->next)
	{
		node->cmd->as.command(node->arguments);
	}

	bool first_iteration = true;
	while (app->accumulator >= dt)
	{
        PROFILE_BEGIN(tick_game);

		tick_game(game, (float)dt);
		app->accumulator -= dt;

		if (first_iteration)
		{
			action_system_clear_sticky_edges();
			first_iteration = false;
		}

        PROFILE_END(tick_game);
	}

	unequip_gamestate();
	unequip_action_system();

    PROFILE_BEGIN(tick_ui);

	rhi_equip_state(app->rhi_state);
	{
		rhi_window_t window = app->rhi_window;
		v2_t client_size = rhi_get_window_client_size(window);

		// needed for r1_report_stats
		r1_equip(app->r1);
		{
			tick_ui(io, app, io->input, client_size, (float)frame_time);
		}
		r1_unequip();

		PROFILE_END(tick_ui);

		rhi_begin_frame();

		r1_equip(app->r1);
		{
			r1_begin_frame();
			render_game(game, window, &the_ui->render_commands);
		}
		r1_unequip();

		rhi_end_frame();
	}
	rhi_unequip_state();

	process_asset_changes();

	io->cursor_locked = !the_ui->has_focus;

	asset_system_unequip();
	unequip_cvar_state();
}

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
