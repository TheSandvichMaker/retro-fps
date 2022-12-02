#include "core/core.h"

#include "render/render.h"
#include "render/render_helpers.h"
#include "render/light_baker.h"

#include "in_game_editor.h"
#include "game.h"
#include "map.h"
#include "ui.h"
#include "intersect.h"

static bool g_debug_lightmaps;
static lum_results_t bake_results;

static void push_poly_wireframe(r_immediate_draw_t *draw_call, map_poly_t *poly, v4_t color)
{
    for (size_t triangle_index = 0; triangle_index < poly->index_count / 3; triangle_index++)
    {
        v3_t a = poly->vertices[poly->indices[3*triangle_index + 0]].pos;
        v3_t b = poly->vertices[poly->indices[3*triangle_index + 1]].pos;
        v3_t c = poly->vertices[poly->indices[3*triangle_index + 2]].pos;

        r_push_line(draw_call, a, b, color);
        r_push_line(draw_call, a, c, color);
        r_push_line(draw_call, b, c, color);
    }
}

static void push_brush_wireframe(r_immediate_draw_t *draw_call, map_brush_t *brush, v4_t color)
{
    for (size_t poly_index = 0; poly_index < brush->poly_count; poly_index++)
    {
        map_poly_t *poly = &brush->polys[poly_index];
        push_poly_wireframe(draw_call, poly, color);
    }
}

static inline map_plane_t *plane_from_poly(map_brush_t *brush, map_poly_t *poly)
{
    map_plane_t *plane = brush->first_plane;

    for (size_t i = 0; i < brush->poly_count; i++)
    {
        if (&brush->polys[i] == poly)
            break;

        plane = plane->next;
    }

    return plane;
}

void update_and_render_in_game_editor(game_io_t *io, world_t *world)
{
    player_t *player = world->player;
    map_t    *map    = world->map;

    camera_t *camera = player->attached_camera;

    static bool show_lightmap_debug_panel = true;

    if (ui_button_pressed(BUTTON_F1))
        show_lightmap_debug_panel = !show_lightmap_debug_panel;

    static map_brush_t *selected_brush = NULL;
    static map_plane_t *selected_plane = NULL;
    static map_poly_t  *selected_poly  = NULL;

    static bool pixel_selection_active = false;
    static rect2i_t selected_pixels = { 0 };

    static bool show_direct_light_rays;
    static bool show_indirect_light_rays;

    static bool fullbright_rays = false;
    static bool no_ray_depth_test = false;

    static int min_display_recursion = 0;
    static int max_display_recursion = 0;

    if (show_lightmap_debug_panel)
    {
        UI_Window(strlit("lightmap debugger panel"), 0, 32, 32, 500, 800)
        {
            v3_t p = player_view_origin(player);
            v3_t d = player_view_direction(player);

            m4x4_t view_matrix = make_view_matrix(camera->p, negate(camera->computed_z), (v3_t){0,0,1});

            v3_t camera_x = {
                view_matrix.e[0][0],
                view_matrix.e[1][0],
                view_matrix.e[2][0],
            };

            v3_t camera_y = {
                view_matrix.e[0][1],
                view_matrix.e[1][1],
                view_matrix.e[2][1],
            };

            v3_t camera_z = {
                view_matrix.e[0][2],
                view_matrix.e[1][2],
                view_matrix.e[2][2],
            };

            v3_t recon_p = negate(v3_add3(mul(view_matrix.e[3][0], camera_x),
                                          mul(view_matrix.e[3][1], camera_y),
                                          mul(view_matrix.e[3][2], camera_z)));

            if (!map->light_baked)
            {
                if (ui_button(strlit("Bake Lighting")).released)
                {
                    bake_lighting(&(lum_params_t) {
                        .arena               = &world->arena,
                        .map                 = world->map,
                        .sun_direction       = make_v3(0.25f, 0.75f, 1),
                        .sun_color           = mul(2.0f, make_v3(1, 1, 0.75f)),
                        .sky_color           = mul(1.0f, make_v3(0.15f, 0.30f, 0.62f)),

                        // TODO: Have a macro for optimization level to check instead of DEBUG
#if DEBUG
                        .ray_count               = 8,
                        .ray_recursion           = 4,
                        .fog_light_sample_count  = 4,
                        .fogmap_scale            = 16,
#else
                        .ray_count               = 64,
                        .ray_recursion           = 4,
                        .fog_light_sample_count  = 64,
                        .fogmap_scale            = 8,
#endif
                    }, &bake_results);
                }
            }

            ui_label(string_format(temp, "camera d:               %.02f %.02f %.02f", d.x, d.y, d.z), 0);
            ui_label(string_format(temp, "camera p:               %.02f %.02f %.02f", p.x, p.y, p.z), 0);
            ui_label(string_format(temp, "reconstructed camera p: %.02f %.02f %.02f", recon_p.x, recon_p.y, recon_p.z), 0);

            ui_label(string_format(temp, "fogmap resolution: %u %u %u", map->fogmap_w, map->fogmap_h, map->fogmap_d), 0);

            ui_checkbox(strlit("enabled"), &g_debug_lightmaps);
            ui_checkbox(strlit("show direct light rays"), &show_direct_light_rays);
            ui_checkbox(strlit("show indirect light rays"), &show_indirect_light_rays);
            ui_checkbox(strlit("fullbright rays"), &fullbright_rays);
            ui_checkbox(strlit("no ray depth test"), &no_ray_depth_test);

            ui_increment_decrement(strlit("min recursion level"), &min_display_recursion, 0, 16);
            ui_increment_decrement(strlit("max recursion level"), &max_display_recursion, 0, 16);

            if (selected_poly)
            {
                map_poly_t *poly = selected_poly;

                texture_desc_t desc;
                render->describe_texture(poly->lightmap, &desc);

                ui_label(string_format(temp, "debug ordinal: %d", selected_plane->debug_ordinal), 0);
                ui_label(string_format(temp, "resolution: %u x %u", desc.w, desc.h), 0);
                if (pixel_selection_active)
                {
                    ui_label(string_format(temp, "selected pixel region: (%d, %d) (%d, %d)", 
                                           selected_pixels.min.x,
                                           selected_pixels.min.y,
                                           selected_pixels.max.x,
                                           selected_pixels.max.y), 0);

                    if (ui_button(strlit("clear selection")).released)
                    {
                        pixel_selection_active = false;
                        selected_pixels = (rect2i_t){ 0 };
                    }
                }
                else
                {
                    ui_spacer(ui_txt(1.0f));
                    ui_spacer(ui_txt(1.0f));
                }

                ui_box_t *image_viewer = ui_box(strlit("lightmap image"), UI_CLICKABLE|UI_DRAGGABLE|UI_DRAW_BACKGROUND);
                if (desc.w >= desc.h)
                {
                    ui_set_size(image_viewer, AXIS2_X, ui_pct(1.0f, 1.0f));
                    ui_set_size(image_viewer, AXIS2_Y, ui_aspect_ratio((float)desc.h / (float)desc.w, 1.0f));
                }
                else
                {
                    ui_set_size(image_viewer, AXIS2_X, ui_aspect_ratio((float)desc.w / (float)desc.h, 1.0f));
                    ui_set_size(image_viewer, AXIS2_Y, ui_pct(1.0f, 1.0f));
                }
                image_viewer->texture = poly->lightmap;

                ui_interaction_t interaction = ui_interaction_from_box(image_viewer);
                if (interaction.hovering || pixel_selection_active)
                {
                    v2_t rel_press_p = sub(interaction.press_p, image_viewer->rect.min);
                    v2_t rel_mouse_p = sub(interaction.mouse_p, image_viewer->rect.min);

                    v2_t rect_dim   = rect2_get_dim(image_viewer->rect);
                    v2_t pixel_size = div(rect_dim, make_v2((float)desc.w, (float)desc.h));

                    UI_Parent(image_viewer)
                    {
                        ui_box_t *selection_highlight = ui_box(strlit("selection highlight"), UI_DRAW_OUTLINE);
                        selection_highlight->style.outline_color = ui_gradient_from_rgba(1, 0, 0, 1);

                        v2_t selection_start = { rel_press_p.x, rel_press_p.y };
                        v2_t selection_end   = { rel_mouse_p.x, rel_mouse_p.y };
                        
                        rect2i_t pixel_selection = { 
                            .min = {
                                (int)(selection_start.x / pixel_size.x),
                                (int)(selection_start.y / pixel_size.y),
                            },
                            .max = {
                                (int)(selection_end.x / pixel_size.x) + 1,
                                (int)(selection_end.y / pixel_size.y) + 1,
                            },
                        };

                        if (pixel_selection.max.y < pixel_selection.min.y)
                            SWAP(int, pixel_selection.max.y, pixel_selection.min.y);

                        if (interaction.dragging)
                        {
                            pixel_selection_active = true;
                            selected_pixels = pixel_selection;
                        }

                        v2i_t selection_dim = rect2i_get_dim(selected_pixels);

                        ui_set_size(selection_highlight, AXIS2_X, ui_pixels((float)selection_dim.x*pixel_size.x, 1.0f));
                        ui_set_size(selection_highlight, AXIS2_Y, ui_pixels((float)selection_dim.y*pixel_size.y, 1.0f));

                        selection_highlight->position_offset[AXIS2_X] = (float)selected_pixels.min.x * pixel_size.x;
                        selection_highlight->position_offset[AXIS2_Y] = rect_dim.y - (float)(selected_pixels.min.y + selection_dim.y) * pixel_size.y;

                    }
                }
            }
        }
    }

    if (g_debug_lightmaps)
    {
        r_command_identifier(strlit("lightmap debug"));
        r_immediate_draw_t *draw_call = r_immediate_draw_begin(&(r_immediate_draw_t){
            .topology   = R_PRIMITIVE_TOPOLOGY_LINELIST,
            .depth_bias = 0.005f,
            .depth_test = !no_ray_depth_test,
            .blend_mode = R_BLEND_ADDITIVE,
        });

        if (io->cursor_locked)
        {
            intersect_result_t intersect;
            if (intersect_map(map, &(intersect_params_t) {
                    .o = player_view_origin(player),
                    .d = player_view_direction(player),
                }, &intersect))
            {
                map_plane_t *plane = plane_from_poly(intersect.brush, intersect.poly);

                if (button_pressed(BUTTON_FIRE1))
                {
                    if (selected_plane == plane)
                    {
                        selected_brush = NULL;
                        selected_plane = NULL;
                        selected_poly  = NULL;
                    }
                    else
                    {
                        selected_brush = intersect.brush;
                        selected_plane = plane;
                        selected_poly  = intersect.poly;
                    }
                }

                if (selected_poly != intersect.poly)
                    push_poly_wireframe(draw_call, intersect.poly, COLORF_RED);
            }
            else
            {
                if (button_pressed(BUTTON_FIRE1))
                {
                    selected_brush = NULL;
                    selected_plane = NULL;
                    selected_poly  = NULL;
                }
            }
        }

        if (selected_plane)
        {
            map_plane_t *plane = selected_plane;

            float scale_x = plane->lm_scale_x;
            float scale_y = plane->lm_scale_y;

            push_poly_wireframe(draw_call, &selected_brush->polys[plane->poly_index], make_v4(0.0f, 0.0f, 0.0f, 0.75f));

            r_push_arrow(draw_call, plane->lm_origin, add(plane->lm_origin, mul(scale_x, plane->lm_s)), make_v4(0.5f, 0.0f, 0.0f, 1.0f));
            r_push_arrow(draw_call, plane->lm_origin, add(plane->lm_origin, mul(scale_y, plane->lm_t)), make_v4(0.0f, 0.5f, 0.0f, 1.0f));

            v3_t square_v0 = add(plane->lm_origin, mul(scale_x, plane->lm_s));
            v3_t square_v1 = add(plane->lm_origin, mul(scale_y, plane->lm_t));
            v3_t square_v2 = v3_add3(plane->lm_origin, mul(scale_x, plane->lm_s), mul(scale_y, plane->lm_t));

            r_push_line(draw_call, square_v0, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));
            r_push_line(draw_call, square_v1, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));

            if (pixel_selection_active)
            {
                texture_desc_t desc;
                render->describe_texture(selected_poly->lightmap, &desc);

                float texscale_x = (float)desc.w;
                float texscale_y = (float)desc.h;

                v2_t selection_worldspace = {
                    scale_x*((float)selected_pixels.min.x / texscale_x),
                    scale_y*((float)selected_pixels.min.y / texscale_y),
                };

                v2i_t selection_dim = rect2i_get_dim(selected_pixels);

                v2_t selection_dim_worldspace = {
                    scale_x*(selection_dim.x / texscale_x),
                    scale_y*(selection_dim.y / texscale_y),
                };

                v3_t pixel_v0 = v3_add3(plane->lm_origin,
                                         mul(selection_worldspace.x, plane->lm_s),
                                         mul(selection_worldspace.y, plane->lm_t));
                v3_t pixel_v1 = add(pixel_v0, mul(selection_dim_worldspace.x, plane->lm_s));
                v3_t pixel_v2 = add(pixel_v0, mul(selection_dim_worldspace.y, plane->lm_t));
                v3_t pixel_v3 = v3_add3(pixel_v0, 
                                        mul(selection_dim_worldspace.x, plane->lm_s), 
                                        mul(selection_dim_worldspace.y, plane->lm_t));

                r_push_line(draw_call, pixel_v0, pixel_v1, COLORF_RED);
                r_push_line(draw_call, pixel_v0, pixel_v2, COLORF_RED);
                r_push_line(draw_call, pixel_v2, pixel_v3, COLORF_RED);
                r_push_line(draw_call, pixel_v1, pixel_v3, COLORF_RED);
            }
        }

        if (show_indirect_light_rays)
        {
            for (lum_path_t *path = bake_results.debug_data.first_path;
                 path;
                 path = path->next)
            {
                if (pixel_selection_active &&
                    !rect2i_contains_exclusive(selected_pixels, path->source_pixel))
                {
                    continue;
                }

                if (path->first_vertex->poly != selected_poly)
                    continue;

                int vertex_index = 0;
                if ((int)path->vertex_count >= min_display_recursion &&
                    (int)path->vertex_count <= max_display_recursion)
                {
                    for (lum_path_vertex_t *vertex = path->first_vertex;
                         vertex;
                         vertex = vertex->next)
                    {
                        if (show_direct_light_rays)
                        {
                            for (size_t sample_index = 0; sample_index < vertex->light_sample_count; sample_index++)
                            {
                                lum_light_sample_t *sample = &vertex->light_samples[sample_index];

                                if (sample->shadow_ray_t > 0.0f && sample->shadow_ray_t < FLT_MAX)
                                {
                                    // r_push_line(draw_call, vertex->o, add(vertex->o, mul(sample->shadow_ray_t, sample->d)), COLORF_RED);
                                }
                                else if (sample->shadow_ray_t == FLT_MAX && sample_index < map->light_count)
                                {
                                    map_point_light_t *point_light = &map->lights[sample_index];

                                    v3_t color = sample->contribution;

                                    if (fullbright_rays)
                                    {
                                        color = normalize(color);
                                    }

#if 0
                                    if (vertex == path->first_vertex)
                                    {
                                        r_push_arrow(draw_call, point_light->p, vertex->o, 
                                                     make_v4(color.x, color.y, color.z, 1.0f));
                                    }
#endif

                                    if (!vertex->next || vertex_index + 2 == (int)path->vertex_count)
                                    {
                                        r_push_line(draw_call, vertex->o, point_light->p, 
                                                    make_v4(color.x, color.y, color.z, 1.0f));
                                    }
                                }
                            }
                        }

                        if (vertex->next)
                        {
                            lum_path_vertex_t *next_vertex = vertex->next;

                            if (next_vertex->brush)
                            {
                                v4_t start_color = make_v4(vertex->contribution.x,
                                                           vertex->contribution.y,
                                                           vertex->contribution.z,
                                                           1.0f);

                                v4_t end_color = make_v4(next_vertex->contribution.x,
                                                         next_vertex->contribution.y,
                                                         next_vertex->contribution.z,
                                                         1.0f);

                                if (fullbright_rays)
                                {
                                    start_color.xyz = normalize(start_color.xyz);
                                    end_color.xyz = normalize(end_color.xyz);
                                }

                                if (vertex == path->first_vertex)
                                {
                                    r_push_arrow_gradient(draw_call, next_vertex->o, vertex->o, end_color, start_color);
                                }
                                else
                                {
                                    r_push_line_gradient(draw_call, vertex->o, next_vertex->o, start_color, end_color);
                                }
                            }
                        }

                        vertex_index += 1;
                    }
                }
            }
        }

        r_immediate_draw_end(draw_call);
    }
}
