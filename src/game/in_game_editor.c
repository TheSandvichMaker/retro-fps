#include "core/core.h"

#include "render/render.h"
#include "render/render_helpers.h"
#include "render/light_baker.h"

#include "in_game_editor.h"
#include "game.h"
#include "map.h"
// #include "ui.h"
#include "debug_ui.h"
#include "intersect.h"
#include "asset.h"
#include "audio.h"

static void push_poly_wireframe(map_t *map, r_immediate_draw_t *draw_call, map_poly_t *poly, v4_t color)
{
    uint16_t *indices   = map->indices          + poly->first_index;
    v3_t     *positions = map->vertex.positions + poly->first_vertex;

    for (size_t triangle_index = 0; triangle_index < poly->index_count / 3; triangle_index++)
    {
        v3_t a = positions[indices[3*triangle_index + 0]];
        v3_t b = positions[indices[3*triangle_index + 1]];
        v3_t c = positions[indices[3*triangle_index + 2]];

        r_push_line(draw_call, a, b, color);
        r_push_line(draw_call, a, c, color);
        r_push_line(draw_call, b, c, color);
    }
}

static void push_brush_wireframe(map_t *map, r_immediate_draw_t *draw_call, map_brush_t *brush, v4_t color)
{
    for (size_t poly_index = 0; poly_index < brush->plane_poly_count; poly_index++)
    {
        map_poly_t *poly = &map->polys[brush->first_plane_poly + poly_index];
        push_poly_wireframe(map, draw_call, poly, color);
    }
}

typedef struct lightmap_editor_state_t
{
    v2_t window_position;

    bool debug_lightmaps;

    map_brush_t *selected_brush;
    map_plane_t *selected_plane;
    map_poly_t  *selected_poly;

    bool pixel_selection_active;
    rect2i_t selected_pixels;

    bool show_direct_light_rays;
    bool show_indirect_light_rays;

    bool fullbright_rays;
    bool no_ray_depth_test;

    int min_display_recursion;
    int max_display_recursion;
} lightmap_editor_state_t;

typedef struct editor_state_t
{
    bool lightmap_editor_enabled;
    lightmap_editor_state_t lightmap_editor;
} editor_state_t;

static editor_state_t g_editor = {
    .lightmap_editor_enabled = true,
    .lightmap_editor = {
        .window_position = { 32, 32 },
    },
};

static void update_and_render_lightmap_editor(game_io_t *io, world_t *world)
{
	(void)io;

    player_t *player = world->player;
    map_t    *map    = world->map;

    map_entity_t *worldspawn = map->worldspawn;

    v3_t sun_color = v3_normalize(v3_from_key(map, worldspawn, S("sun_color")));
    float sun_brightness = float_from_key(map, worldspawn, S("sun_brightness"));
    sun_color = mul(sun_brightness, sun_color);

    v3_t ambient_color = v3_from_key(map, worldspawn, S("ambient_color"));
    ambient_color = mul(1.0f / 255.0f, ambient_color);

    camera_t *camera = player->attached_camera;
	(void)camera;

    lightmap_editor_state_t *lm_editor = &g_editor.lightmap_editor;
	(void)lm_editor;

	waveform_t *test_sound = get_waveform_from_string(S("gamedata/audio/menu_select.wav"));

	r_push_view_screenspace();

	ui_window_begin(S("Lightmap Editor"), rect2_min_dim(make_v2(32.0f, 128.0f), make_v2(512.0f, 512.0f)));
	{
		rect2_t *layout = ui_layout_rect();

		ui_panel_begin(ui_cut_top(layout, 18.0f));
		{
			rect2_t *sub_layout = ui_layout_rect();
			ui_set_layout_direction(UI_CUT_LEFT);

			float width = ui_divide_space(5);
			for (int i = 0; i < 5; i++)
			{
				ui_set_next_rect(ui_cut_left(sub_layout, width));
				ui_button(Sf("Butt #%d", i));
			}
		}
		ui_panel_end();

		static ui_cut_side_t side = UI_CUT_TOP;

		if (ui_button(S("Next cut direction")))
		{
			side = ((int)side + 1) % UI_CUT_COUNT;
		}
		ui_set_layout_direction(side);

		if (ui_button(S("Test Button")))
		{
			play_sound(&(play_sound_t){
				.waveform = test_sound,
				.volume   = 1.0f,
			});
		}

		static float slide_this = 10.0f;
		ui_slider(Sf("slide this %.02f", slide_this), &slide_this, 5.0f, 25.0f);

		for (int i = 0; i < 64; i++)
		{
			ui_button(Sf("Test Button #%d", i));
		}
	}
	ui_window_end();

	r_pop_view();

#if 0
    ui_box_t *window_panel = ui_box(strlit("Lightmap Editor##panel"), UI_DRAW_BACKGROUND|UI_DRAW_OUTLINE);

    float window_width  = 512;
    float window_height = 512;

    window_panel->position_offset[AXIS2_X] = lm_editor->window_position.x;
    window_panel->position_offset[AXIS2_Y] = lm_editor->window_position.y;

    window_panel->semantic_size[AXIS2_X] = (ui_size_t){ UI_SIZE_PIXELS, window_width, 1.0f };
    window_panel->semantic_size[AXIS2_Y] = (ui_size_t){ UI_SIZE_PIXELS, window_height, 1.0f };
    window_panel->rect.min = lm_editor->window_position;

    ui_push_parent(window_panel);

    ui_box_t *container = ui_box(strnull, 0);
    ui_set_size(container, AXIS2_Y, ui_txt(1.0f));
    container->layout_axis = AXIS2_X;

    UI_Parent(container)
    {
        ui_style_t style = ui_get_style();

        style.outline_color        = ui_gradient_from_v4(make_v4(0.45f, 0.25f, 0.25f, 1.0f));
        style.background_color     = ui_gradient_vertical(make_v4(0.35f, 0.15f, 0.15f, 1.0f), make_v4(0.25f, 0.10f, 0.10f, 1.0f));
        style.background_color_hot = ui_gradient_vertical(mul(1.5f, make_v4(0.35f, 0.15f, 0.15f, 1.0f)), mul(1.5f, make_v4(0.25f, 0.10f, 0.10f, 1.0f)));

        ui_push_style(&style);

        ui_box_t *title_bar = ui_box(strlit("Lightmap Editor"), UI_DRAW_BACKGROUND|UI_DRAW_TEXT|UI_DRAGGABLE|UI_CLICKABLE);
        ui_set_size(title_bar, AXIS2_X, ui_pct(1.0f, 0.0f));

        if (ui_button(strlit("Close")).released)
        {
            g_editor.lightmap_editor_enabled = false;
        }

        ui_interaction_t title_interaction = ui_interaction_from_box(title_bar);
        if (title_interaction.dragging)
        {
            lm_editor->window_position = add(lm_editor->window_position, title_interaction.drag_delta);

            v2_t bounds = ui_get_screen_bounds();
            bounds.x -= window_width;
            bounds.y -= window_height;

            if (lm_editor->window_position.x < 0) lm_editor->window_position.x = 0.0f;
            if (lm_editor->window_position.y < 0) lm_editor->window_position.y = 0.0f;
            if (lm_editor->window_position.x > bounds.x) lm_editor->window_position.x = bounds.x;
            if (lm_editor->window_position.y > bounds.y) lm_editor->window_position.y = bounds.y;
        }

        ui_pop_style();
    }

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

    ui_label(string_format(temp, "camera d:               %.02f %.02f %.02f", d.x, d.y, d.z), 0);
    ui_label(string_format(temp, "camera p:               %.02f %.02f %.02f", p.x, p.y, p.z), 0);
    ui_label(string_format(temp, "reconstructed camera p: %.02f %.02f %.02f", recon_p.x, recon_p.y, recon_p.z), 0);

    ui_spacer(ui_txt(1.0f));

	if (ui_button(strlit("Bake Lighting")).released)
	{
		if (map->lightmap_state)
		{
			if (release_bake_state(map->lightmap_state))
			{
				map->lightmap_state = NULL;
			}
		}

		if (!map->lightmap_state)
		{
			// figure out the solid sky color from the fog
			float absorption = map->fog_absorption;
			float density    = map->fog_density;
			float scattering = map->fog_scattering;
			v3_t sky_color = mul(sun_color, (1.0f / (4.0f*PI32))*scattering*density / (density*(scattering + absorption)));

			if (map->lightmap_state)
			{
				release_bake_state(map->lightmap_state);
			}

			map->lightmap_state = bake_lighting(&(lum_params_t) {
				.map                 = map,
				.sun_direction       = make_v3(0.25f, 0.75f, 1),
				.sun_color           = sun_color,
				.sky_color           = sky_color,

				.use_dynamic_sun_shadows = true,

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
			});
		}
	}

	if (!map->lightmap_state)
		ui_label(strlit("Lightmaps have not been baked!"), 0);

	if (map->lightmap_state && map->lightmap_state->finalized)
	{
		if (ui_button(strlit("Clear Lightmaps")).released)
		{
			for (size_t poly_index = 0; poly_index < map->poly_count; poly_index++)
			{
				map_poly_t *poly = &map->polys[poly_index];
				if (RESOURCE_HANDLE_VALID(poly->lightmap))
				{
					render->destroy_texture(poly->lightmap);
					poly->lightmap = NULL_RESOURCE_HANDLE;
				}
			}

			render->destroy_texture(map->fogmap);
			map->fogmap = NULL_RESOURCE_HANDLE;

			release_bake_state(map->lightmap_state);
			map->lightmap_state = NULL;
		}
	}

	if (map->lightmap_state)
	{
		lum_bake_state_t *state = map->lightmap_state;

		if (state->finalized)
		{
			double time_elapsed = state->final_bake_time;
			int minutes = (int)floor(time_elapsed / 60.0);
			int seconds = (int)floor(time_elapsed - 60.0*minutes);
			int microseconds = (int)floor(1000.0*(time_elapsed - 60.0*minutes - seconds));
			ui_label(string_format(temp, "total bake time:  %02u:%02u:%03u", minutes, seconds, microseconds), 0);

			ui_label(string_format(temp, "fogmap resolution: %u %u %u", map->fogmap_w, map->fogmap_h, map->fogmap_d), 0);

			ui_checkbox(strlit("enabled"), &lm_editor->debug_lightmaps);
			ui_checkbox(strlit("show direct light rays"), &lm_editor->show_direct_light_rays);
			ui_checkbox(strlit("show indirect light rays"), &lm_editor->show_indirect_light_rays);
			ui_checkbox(strlit("fullbright rays"), &lm_editor->fullbright_rays);
			ui_checkbox(strlit("no ray depth test"), &lm_editor->no_ray_depth_test);

			ui_increment_decrement(strlit("min recursion level"), &lm_editor->min_display_recursion, 0, 16);
			ui_increment_decrement(strlit("max recursion level"), &lm_editor->max_display_recursion, 0, 16);

			if (lm_editor->selected_poly)
			{
				map_poly_t *poly = lm_editor->selected_poly;

				texture_desc_t desc;
				render->describe_texture(poly->lightmap, &desc);

				ui_label(string_format(temp, "resolution: %u x %u", desc.w, desc.h), 0);
				if (lm_editor->pixel_selection_active)
				{
					ui_label(string_format(temp, "selected pixel region: (%d, %d) (%d, %d)", 
										   lm_editor->selected_pixels.min.x,
										   lm_editor->selected_pixels.min.y,
										   lm_editor->selected_pixels.max.x,
										   lm_editor->selected_pixels.max.y), 0);

					if (ui_button(strlit("clear selection")).released)
					{
						lm_editor->pixel_selection_active = false;
						lm_editor->selected_pixels = (rect2i_t){ 0 };
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
				if (interaction.hovering || lm_editor->pixel_selection_active)
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
							lm_editor->pixel_selection_active = true;
							lm_editor->selected_pixels = pixel_selection;
						}

						v2i_t selection_dim = rect2i_get_dim(lm_editor->selected_pixels);

						ui_set_size(selection_highlight, AXIS2_X, ui_pixels((float)selection_dim.x*pixel_size.x, 1.0f));
						ui_set_size(selection_highlight, AXIS2_Y, ui_pixels((float)selection_dim.y*pixel_size.y, 1.0f));

						selection_highlight->position_offset[AXIS2_X] = (float)lm_editor->selected_pixels.min.x * pixel_size.x;
						selection_highlight->position_offset[AXIS2_Y] = rect_dim.y - (float)(lm_editor->selected_pixels.min.y + selection_dim.y) * pixel_size.y;
					}
				}
			}
		}
		else
		{
			float progress = 100.0f*bake_progress(map->lightmap_state);
			ui_label(string_format(temp, "bake progress: %u / %u (%.02f%%)", map->lightmap_state->jobs_completed, map->lightmap_state->job_count, progress), 0);
			hires_time_t current_time = os_hires_time();
			double time_elapsed = os_seconds_elapsed(map->lightmap_state->start_time, current_time);
			int minutes = (int)floor(time_elapsed / 60.0);
			int seconds = (int)floor(time_elapsed - 60.0*minutes);
			ui_label(string_format(temp, "time elapsed:  %02u:%02u", minutes, seconds), 0);
		}
    }

    ui_pop_parent();

    if (lm_editor->debug_lightmaps)
    {
        r_command_identifier(strlit("lightmap debug"));
        r_immediate_draw_t *draw_call = r_immediate_draw_begin(&(r_immediate_params_t){
            .topology   = R_PRIMITIVE_TOPOLOGY_LINELIST,
            .depth_bias = 0.005f,
            .depth_test = !lm_editor->no_ray_depth_test,
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
                map_plane_t *plane = intersect.plane;

                if (button_pressed(BUTTON_FIRE1))
                {
                    if (lm_editor->selected_plane == plane)
                    {
                        lm_editor->selected_brush = NULL;
                        lm_editor->selected_plane = NULL;
                        lm_editor->selected_poly  = NULL;
                    }
                    else
                    {
                        lm_editor->selected_brush = intersect.brush;
                        lm_editor->selected_plane = plane;
                        lm_editor->selected_poly  = intersect.poly;
                    }
                }

                if (lm_editor->selected_poly != intersect.poly)
                    push_poly_wireframe(map, draw_call, intersect.poly, COLORF_RED);
            }
            else
            {
                if (button_pressed(BUTTON_FIRE1))
                {
                    lm_editor->selected_brush = NULL;
                    lm_editor->selected_plane = NULL;
                    lm_editor->selected_poly  = NULL;
                }
            }
        }

        if (lm_editor->selected_plane)
        {
            map_plane_t *plane = lm_editor->selected_plane;

            float scale_x = plane->lm_scale_x;
            float scale_y = plane->lm_scale_y;

            push_poly_wireframe(map, draw_call, lm_editor->selected_poly, make_v4(0.0f, 0.0f, 0.0f, 0.75f));

            r_push_arrow(draw_call, plane->lm_origin, add(plane->lm_origin, mul(scale_x, plane->lm_s)), make_v4(0.5f, 0.0f, 0.0f, 1.0f));
            r_push_arrow(draw_call, plane->lm_origin, add(plane->lm_origin, mul(scale_y, plane->lm_t)), make_v4(0.0f, 0.5f, 0.0f, 1.0f));

            v3_t square_v0 = add(plane->lm_origin, mul(scale_x, plane->lm_s));
            v3_t square_v1 = add(plane->lm_origin, mul(scale_y, plane->lm_t));
            v3_t square_v2 = v3_add3(plane->lm_origin, mul(scale_x, plane->lm_s), mul(scale_y, plane->lm_t));

            r_push_line(draw_call, square_v0, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));
            r_push_line(draw_call, square_v1, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));

            if (lm_editor->pixel_selection_active)
            {
                texture_desc_t desc;
                render->describe_texture(lm_editor->selected_poly->lightmap, &desc);

                float texscale_x = (float)desc.w;
                float texscale_y = (float)desc.h;

                v2_t selection_worldspace = {
                    scale_x*((float)lm_editor->selected_pixels.min.x / texscale_x),
                    scale_y*((float)lm_editor->selected_pixels.min.y / texscale_y),
                };

                v2i_t selection_dim = rect2i_get_dim(lm_editor->selected_pixels);

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

		if (map->lightmap_state && map->lightmap_state->finalized)
		{
			lum_debug_data_t *debug_data = &map->lightmap_state->results.debug;

			if (lm_editor->show_indirect_light_rays)
			{
				for (lum_path_t *path = debug_data->first_path;
					 path;
					 path = path->next)
				{
					if (lm_editor->pixel_selection_active &&
						!rect2i_contains_exclusive(lm_editor->selected_pixels, path->source_pixel))
					{
						continue;
					}

					if (path->first_vertex->poly != lm_editor->selected_poly)
						continue;

					int vertex_index = 0;
					if ((int)path->vertex_count >= lm_editor->min_display_recursion &&
						(int)path->vertex_count <= lm_editor->max_display_recursion)
					{
						for (lum_path_vertex_t *vertex = path->first_vertex;
							 vertex;
							 vertex = vertex->next)
						{
							if (lm_editor->show_direct_light_rays)
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

										if (lm_editor->fullbright_rays)
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

									if (lm_editor->fullbright_rays)
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
		}

        r_immediate_draw_end(draw_call);
    }
#endif
}

void update_and_render_in_game_editor(game_io_t *io, world_t *world)
{
    if (ui_button_pressed(BUTTON_F1))
        g_editor.lightmap_editor_enabled = !g_editor.lightmap_editor_enabled;

    if (g_editor.lightmap_editor_enabled)
        update_and_render_lightmap_editor(io, world);
}
