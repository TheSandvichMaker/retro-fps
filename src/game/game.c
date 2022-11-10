#include <stdlib.h>

#include "core/core.h"

#include "game.h"
#include "map.h"
#include "input.h"
#include "asset.h"
#include "intersect.h"
#include "ui.h"
#include "render/light_baker.h"
#include "render/render.h"
#include "render/diagram.h"

static bool initialized = false;

static world_t *world;
static resource_handle_t skybox;
static bitmap_font_t font;

#define MAX_VERTICES (8192)
static size_t map_vertex_count;
static v3_t map_vertices[MAX_VERTICES];

static bake_light_results_t bake_results;

void game_init(void)
{
    {
        image_t font_image = load_image(temp, strlit("gamedata/textures/font.png"));
        font.w = font_image.w;
        font.h = font_image.h;
        font.cw = 10;
        font.ch = 12;
        font.texture = render->upload_texture(&(upload_texture_t) {
            .format = PIXEL_FORMAT_RGBA8,
            .w      = font_image.w,
            .h      = font_image.h,
            .pitch  = font_image.pitch,
            .pixels = font_image.pixels,
        });
    }

    world = m_bootstrap(world_t, arena);
    world->map = load_map(&world->arena, strlit("gamedata/maps/test.map"));

#if 1
    bake_lighting(&(bake_light_params_t) {
        .arena         = &world->arena,
        .map           = world->map,
        .sun_direction = make_v3(0.25f, 0.75f, 1),
        .sun_color     = mul(1.0f, make_v3(4, 4, 3.5f)),
        .ambient_color = mul(1.0f, make_v3(0.15f, 0.30f, 0.62f)),

        // TODO: Have a macro for optimization level to check instead of DEBUG
#if DEBUG
        .ray_count     = 8,
        .ray_recursion = 2,
#else
        .ray_count     = 128,
        .ray_recursion = 8,
#endif
    }, &bake_results);
#endif

    world->player = m_alloc_struct(&world->arena, player_t);

    for (map_entity_t *e = world->map->first_entity; e; e = e->next)
    {
        if (is_class(e, strlit("worldspawn")))
        {
            string_t skytex = value_from_key(e, strlit("skytex"));
            m_scoped(temp)
            {
                // TODO: Distinguish between cubemap formats (single image or separate faces)
#if 0
                image_t skytex_img = load_image(temp, string_format(temp, "gamedata/textures/sky/%.*s.png", strexpand(skytex)));
                if (skytex_img.pixels)
                {
                    image_t faces[6];
                    if (split_image_into_cubemap_faces(skytex_img, faces))
                    {
                        skybox = render->upload_texture(&(upload_texture_t){
                            .format = PIXEL_FORMAT_SRGB8_A8,
                            .w     = faces[0].w,
                            .h     = faces[0].h,
                            .pitch = faces[0].pitch,
                            .flags = TEXTURE_FLAG_CUBEMAP,
                            .faces = {
                                faces[0].pixels,
                                faces[1].pixels,
                                faces[2].pixels,
                                faces[3].pixels,
                                faces[4].pixels,
                                faces[5].pixels,
                            },
                        });
                    }
                }
#else
                image_t faces[6] = {
                    load_image(temp, string_format(temp, "gamedata/textures/sky/%.*s/posx.jpg", strexpand(skytex))),
                    load_image(temp, string_format(temp, "gamedata/textures/sky/%.*s/negx.jpg", strexpand(skytex))),
                    load_image(temp, string_format(temp, "gamedata/textures/sky/%.*s/posy.jpg", strexpand(skytex))),
                    load_image(temp, string_format(temp, "gamedata/textures/sky/%.*s/negy.jpg", strexpand(skytex))),
                    load_image(temp, string_format(temp, "gamedata/textures/sky/%.*s/posz.jpg", strexpand(skytex))),
                    load_image(temp, string_format(temp, "gamedata/textures/sky/%.*s/negz.jpg", strexpand(skytex))),
                };

                skybox = render->upload_texture(&(upload_texture_t){
                    .format = PIXEL_FORMAT_SRGB8_A8,
                    .w     = faces[0].w,
                    .h     = faces[0].h,
                    .pitch = faces[0].pitch,
                    .flags = TEXTURE_FLAG_CUBEMAP,
                    .faces = {
                        faces[0].pixels,
                        faces[1].pixels,
                        faces[2].pixels,
                        faces[3].pixels,
                        faces[4].pixels,
                        faces[5].pixels,
                    },
                });
#endif
            }
        }
        else if (is_class(e, strlit("info_player_start")))
        {
            world->player->p = v3_from_key(e, strlit("origin"));
            world->player->p.z += 10.0f;
        }
    }

    initialized = true;
}

void player_mouse_move(player_t *player, float dt)
{
    int dxi, dyi;
    get_mouse_delta(&dxi, &dyi);

    float dx = (float)dxi;
    float dy = (float)dyi;

    float look_speed_x = 10.0f;
    float look_speed_y = 10.0f;

    player->look_yaw   -= look_speed_x*dt*dx;
    player->look_pitch += look_speed_y*dt*dy;

    player->look_yaw   = fmodf(player->look_yaw, 360.0f);
    player->look_pitch = CLAMP(player->look_pitch, -85.0f, 85.0f);
}

void player_freecam(player_t *player, float dt)
{
    player->support = NULL;

    float move_speed = 200.0f;
    v3_t move_delta = { 0 };

    if (button_down(BUTTON_RUN))      move_speed *= 2.0f;

    if (button_down(BUTTON_FORWARD))  move_delta.x += move_speed;
    if (button_down(BUTTON_BACK))     move_delta.x -= move_speed;
    if (button_down(BUTTON_LEFT))     move_delta.y += move_speed;
    if (button_down(BUTTON_RIGHT))    move_delta.y -= move_speed;

    quat_t qpitch = make_quat_axis_angle((v3_t){ 0, 1, 0 }, DEG_TO_RAD*player->look_pitch);
    quat_t qyaw   = make_quat_axis_angle((v3_t){ 0, 0, 1 }, DEG_TO_RAD*player->look_yaw);

    move_delta = quat_rotatev(qpitch, move_delta);
    move_delta = quat_rotatev(qyaw,   move_delta);

    if (button_down(BUTTON_JUMP))     move_delta.z += move_speed;
    if (button_down(BUTTON_CROUCH))   move_delta.z -= move_speed;

    player->dp = move_delta;
    player->p = add(player->p, mul(dt, player->dp));
}

void player_movement(player_t *player, float dt)
{
    quat_t qyaw = make_quat_axis_angle((v3_t){ 0, 0, 1 }, DEG_TO_RAD*player->look_yaw);

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

    for (map_entity_t *entity = world->map->first_entity;
         entity;
         entity = entity->next)
    {
        for (map_brush_t *brush = entity->first_brush;
             brush;
             brush = brush->next)
        {
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

    move_delta = quat_rotatev(qyaw, move_delta);
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
        float shit = dot(normalize_or_zero(move_delta), normalize_or_zero(player->dp));
        float fuck = max(0.0f, 1.0f - shit);
        move_delta = mul(move_delta, fuck);
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

        for (map_entity_t *entity = world->map->first_entity;
             entity;
             entity = entity->next)
        {
            for (map_brush_t *brush = entity->first_brush;
                 brush;
                 brush = brush->next)
            {
                rect3_t bounds = rect3_add(player_bounds, brush->bounds);

#if 0
                diag_add_box(&(diag_box_t){
                    .bounds = brush->bounds,
                    .color  = (brush == player->support ? (v3_t){ 0, 1, 0 } : (v3_t){ 1, 0, 0 }),
                });
#endif

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
}

static v3_t player_view_origin(player_t *player)
{
    v3_t result = player->p;
    float player_height = lerp(56.0f, 32.0f, smootherstep(player->crouch_t));
    result.z += player_height;
    return result;
}

static v3_t player_view_direction(player_t *player)
{
    quat_t qpitch = make_quat_axis_angle((v3_t){ 0, 1, 0 }, DEG_TO_RAD*player->look_pitch);
    quat_t qyaw   = make_quat_axis_angle((v3_t){ 0, 0, 1 }, DEG_TO_RAD*player->look_yaw);

    v3_t dir = { 1, 0, 0 };
    dir = quat_rotatev(qpitch, dir);
    dir = quat_rotatev(qyaw,   dir);

    return dir;
}

static bool g_debug_lightmaps;
static bool g_cursor_locked;

static void draw_brush_wireframe(map_brush_t *brush, uint32_t color)
{
    for (size_t poly_index = 0; poly_index < brush->poly_count; poly_index++)
    {
        map_poly_t *poly = &brush->polys[poly_index];
        for (size_t triangle_index = 0; triangle_index < poly->index_count / 3; triangle_index++)
        {
            v3_t a = poly->vertices[poly->indices[3*triangle_index + 0]].pos;
            v3_t b = poly->vertices[poly->indices[3*triangle_index + 1]].pos;
            v3_t c = poly->vertices[poly->indices[3*triangle_index + 2]].pos;

            r_immediate_line(a, b, color);
            r_immediate_line(a, c, color);
            r_immediate_line(b, c, color);
        }
    }
}

void game_tick(game_io_t *io, float dt)
{
    if (!initialized)
    {
        game_init();
    }

    update_input_state(io->input_state);

    // I suppose for UI eating input, I will just check if the UI is focused and then
    // stop any game code from seeing the input.

    bool ui_focused = ui_begin(&font);

    if (ui_focused)
    {
        // no more input for you, game
        // but the UI code still needs to see input
        // the way I did a lot of the code base for this project is sort of different
        // from how I usually do things, just to keep me on my toes, but as a result
        // a lot of it is kind of... bad. the input included. it receives a game_io_t
        // struct from the platform layer just like in handmade hero, but then instead
        // of passing down the input state to functions that need it, update_input_state
        // will set some global stuff which can then be accessed anywhere in the code,
        // so there is no obvious way to control who gets to query input.

        // instead it will be just, hey, before any other game code runs, maybe eat the
        // inputs.

        suppress_game_input(true);
    }

    if (button_pressed(BUTTON_FIRE2))
        g_cursor_locked = !g_cursor_locked;

    io->cursor_locked = g_cursor_locked;

    player_t *player = world->player;

    if (button_pressed(BUTTON_TOGGLE_NOCLIP))
    {
        if (player->move_mode == PLAYER_MOVE_NORMAL)
            player->move_mode = PLAYER_MOVE_FREECAM;
        else
            player->move_mode = PLAYER_MOVE_NORMAL;
    }

    if (io->cursor_locked)
        player_mouse_move(player, dt);

    switch (player->move_mode)
    {
        case PLAYER_MOVE_NORMAL:  player_movement(player, dt); break;
        case PLAYER_MOVE_FREECAM: player_freecam(player, dt); break;
    }

    {
        // set up view

        r_view_t view;
        r_default_view(&view);

        view.skybox = skybox;

        v3_t p = player_view_origin(player);
        v3_t d = player_view_direction(player);

        v3_t up  = { 0, 0, 1 };
        view.camera = make_view_matrix(p, d, up);

        int w, h;
        render->get_resolution(&w, &h);

        float aspect = (float)w / (float)h;
        view.projection = make_perspective_matrix(60.0f, aspect, 1.0f);

        r_push_view(&view);
    }

    // render map geometry

    map_t *map = world->map;

    for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
    {
        map_brush_t *brush = map->brushes[brush_index];

        for (size_t poly_index = 0; poly_index < brush->poly_count; poly_index++)
        {
            map_poly_t *poly = &brush->polys[poly_index];

            r_submit_command(&(r_command_model_t) {
                .base = {
                    .kind = R_COMMAND_MODEL,
                },
                .model     = poly->mesh,
                .texture   = poly->texture,
                .lightmap  = poly->lightmap,
                .transform = m4x4_identity,
            });
        }
    }

    static map_brush_t *selected_brush = NULL;

    rect2_t panel_bounds = {
        .min = { 32, 32  },
        .max = { 512, 256 },
    };

    if (g_debug_lightmaps)
    {
        r_immediate_topology(R_PRIMITIVE_TOPOLOGY_LINELIST);
        r_immediate_depth_bias(0.005f);

        // r_immediate_line(make_v3(0, 0, 0), make_v3(0, 0, 1), make_v3(1, 0, 0));

        intersect_result_t intersect;
        if (intersect_map(map, &(intersect_params_t) {
                .o = player_view_origin(player),
                .d = player_view_direction(player),
            }, &intersect))
        {
            if (button_pressed(BUTTON_FIRE1))
                selected_brush = intersect.brush;

            if (!selected_brush)
                draw_brush_wireframe(intersect.brush, COLOR32_RED);
        }
        else
        {
            if (button_pressed(BUTTON_FIRE1))
                selected_brush = NULL;
        }

        if (selected_brush)
        {
            for (map_plane_t *plane = selected_brush->first_plane;
                 plane;
                 plane = plane->next)
            {
                r_immediate_arrow(plane->lm_origin, add(plane->lm_origin, mul(10.0f, plane->s.xyz)), COLOR32_RED);
                r_immediate_arrow(plane->lm_origin, add(plane->lm_origin, mul(10.0f, plane->t.xyz)), COLOR32_BLUE);

                v3_t square_v0 = add(plane->lm_origin, mul(2.5f, plane->s.xyz));
                v3_t square_v1 = add(plane->lm_origin, mul(2.5f, plane->t.xyz));
                v3_t square_v2 = v3_add3(plane->lm_origin, mul(2.5f, plane->s.xyz), mul(2.5f, plane->t.xyz));

                r_immediate_line(square_v0, square_v2, COLOR32_GREEN);
                r_immediate_line(square_v1, square_v2, COLOR32_GREEN);
            }

            draw_brush_wireframe(selected_brush, COLOR32_GREEN);
        }

        for (bake_light_debug_ray_t *ray = bake_results.debug_data.direct_rays.first;
             ray;
             ray = ray->next)
        {
            if (ray->spawn_brush == selected_brush)
            {
                if (ray->t != FLT_MAX)
                {
                    r_immediate_arrow(ray->o, add(ray->o, mul(ray->t, ray->d)), COLOR32_RED);
                }
                else
                {
                    r_immediate_arrow(ray->o, add(ray->o, mul(15.0f, ray->d)), COLOR32_GREEN);
                }
            }
        }

        r_command_identifier(strlit("lightmap debug"));
        r_immediate_flush();
    }

    ui_begin_panel(panel_bounds);
    {
        if (ui_button(strlit("lightmap debugger")))
        {
            g_debug_lightmaps = !g_debug_lightmaps;
        }
    }
    ui_end_panel();

#if 0
    diag_node_t *bvh_diag = diag_begin(strlit("bvh"));

    for (size_t i = 0; i < map->node_count; i++)
    {
        map_bvh_node_t *node = &map->nodes[i];

        diag_add_box(bvh_diag, r_debug_color(i), node->bounds);
    }
#endif

    // diag_draw_all(&font);

    ui_end();

    if (button_pressed(BUTTON_ESCAPE))
    {
        io->exit_requested = true;
    }
}
