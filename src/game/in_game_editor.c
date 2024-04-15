#include "core/core.h"

#include "render.h"
#include "render_helpers.h"
#include "light_baker.h"
#include "in_game_editor.h"
#include "game.h"
#include "map.h"
#include "ui.h"
#include "intersect.h"
#include "asset.h"
#include "audio.h"
#include "mesh.h"
#include "convex_hull_debugger.h"

static void push_poly_wireframe(r_context_t *rc, map_t *map, map_poly_t *poly, v4_t color)
{
    uint16_t *indices   = map->indices          + poly->first_index;
    v3_t     *positions = map->vertex.positions + poly->first_vertex;

    for (size_t triangle_index = 0; triangle_index < poly->index_count / 3; triangle_index++)
    {
        v3_t a = positions[indices[3*triangle_index + 0]];
        v3_t b = positions[indices[3*triangle_index + 1]];
        v3_t c = positions[indices[3*triangle_index + 2]];

        r_immediate_line(rc, a, b, color);
        r_immediate_line(rc, a, c, color);
        r_immediate_line(rc, b, c, color);
    }
}

static void push_brush_wireframe(r_context_t *rc, map_t *map, map_brush_t *brush, v4_t color)
{
    for (size_t poly_index = 0; poly_index < brush->plane_poly_count; poly_index++)
    {
        map_poly_t *poly = &map->polys[brush->first_plane_poly + poly_index];
        push_poly_wireframe(rc, map, poly, color);
    }
}

typedef enum editor_kind_t
{
	EDITOR_TIMINGS,
	EDITOR_LIGHTMAP,
	EDITOR_CONVEX_HULL,
	EDITOR_UI_DEMO,
	EDITOR_COUNT,
} editor_kind_t;

static void ui_demo_proc         (ui_window_t *);
static void lightmap_editor_proc (ui_window_t *);

typedef struct ui_demo_panel_t
{
	ui_window_t window;
	float slider_f32;
	int   slider_i32;
	char             edit_buffer_storage[256];
	dynamic_string_t edit_buffer;
} ui_demo_panel_t;

typedef struct lightmap_editor_state_t
{
	ui_window_t window;

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
	bool initialized;

	bool console_open;

    rect2_t *fullscreen_layout;
    float bar_openness;
	bool pin_bar;

    bool show_timings;

    bool lightmap_editor_enabled;
    lightmap_editor_state_t lm_editor;

	bool hull_test_panel_enabled;
	convex_hull_debugger_t convex_hull_debugger;

	ui_demo_panel_t ui_demo;
} editor_state_t;

static editor_state_t editor = {
    .show_timings = false,

    .lm_editor = {
		.window = {
			.title     = Sc("Lightmap Editor"),
			.draw_proc = lightmap_editor_proc,
		},

		.min_display_recursion = 0,
		.max_display_recursion = 10,
    },

	.convex_hull_debugger = {
		.window = {
			.title     = Sc("Convex Hull Debugger"),
			.draw_proc = convex_hull_debugger_window_proc,
		},

		.automatically_recalculate_hull = true,
		.random_seed              = 1,
		.point_count              = 12,
		.show_points              = true,
		.show_processed_edge      = true,
		.show_new_triangle        = true,
		.show_triangles           = true,
		.show_duplicate_triangles = true,
		.show_wireframe           = true,
		.test_tetrahedron_index   = -1,
		.current_step_index       = 9999,

		.brute_force_min_point_count            = 4,
		.brute_force_max_point_count            = 256,
		.brute_force_iterations_per_point_count = 4096,
	},

	.ui_demo = {
		.window = {
			.title     = Sc("UI Demo Panel"),
			.draw_proc = ui_demo_proc,
		},
	},
};

static void lightmap_editor_proc(ui_window_t *window)
{
	(void)window;

    world_t *world = g_world;
    map_t *map = world->map;

    map_entity_t *worldspawn = map->worldspawn;

    v3_t  sun_color      = v3_normalize(v3_from_key(map, worldspawn, S("sun_color")));
    float sun_brightness = float_from_key(map, worldspawn, S("sun_brightness"));

    sun_color = mul(sun_brightness, sun_color);

    v3_t ambient_color = v3_from_key(map, worldspawn, S("ambient_color"));
    ambient_color = mul(1.0f / 255.0f, ambient_color);

    lightmap_editor_state_t *lm_editor = &editor.lm_editor;

	if (!map->lightmap_state || !map->lightmap_state->finalized)
	{
		ui_header(S("Bake Quality"));

		static int bake_preset              = 0;
		static int ray_count                = 2;
		static int ray_recursion            = 2;
		static int fog_light_sample_count   = 2;
		static int fogmap_scale_index       = 1;
		static bool use_dynamic_sun_shadows = true;

		static string_t preset_labels[] = { Sc("Crappy"), Sc("Acceptable"), Sc("Excessive") };
		if (ui_option_buttons(S("Preset"), &bake_preset, ARRAY_COUNT(preset_labels), preset_labels))
		{
			switch (bake_preset)
			{
				case 0:
				{
					ray_count              = 2;
					ray_recursion          = 2;
					fog_light_sample_count = 2;
					fogmap_scale_index     = 1;
				} break;

				case 1:
				{
					ray_count              = 8;
					ray_recursion          = 3;
					fog_light_sample_count = 4;
					fogmap_scale_index     = 1;
				} break;

				case 2:
				{
					ray_count              = 16;
					ray_recursion          = 4;
					fog_light_sample_count = 8;
					fogmap_scale_index     = 2;
				} break;
			}
		}

		ui_slider_int(S("Rays Per Pixel"), &ray_count, 1, 32);
		ui_slider_int(S("Max Recursion"), &ray_recursion, 1, 8);
		ui_slider_int(S("Fog Light Sample Count"), &fog_light_sample_count, 1, 8);

		static int fogmap_scales[] = {
			32, 16, 8, 4,
		};

		static string_t fogmap_scale_labels[] = {
			Sc("32"), Sc("16"), Sc("8"), Sc("4"),
		};

		ui_option_buttons(S("Fogmap Scale"), &fogmap_scale_index, ARRAY_COUNT(fogmap_scales), fogmap_scale_labels);

		int actual_fogmap_scale = fogmap_scales[fogmap_scale_index];

		ui_checkbox(S("Dynamic Sun Shadows"), &use_dynamic_sun_shadows);

		ui_id_t bake_cancel_button = ui_id(S("Bake Lighting / Cancel"));

		if (!map->lightmap_state)
		{
			ui_set_next_id(bake_cancel_button);
			if (ui_button(S("Bake Lighting")))
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

					.use_dynamic_sun_shadows = use_dynamic_sun_shadows,

					.ray_count               = ray_count,
					.ray_recursion           = ray_recursion,
					.fog_light_sample_count  = fog_light_sample_count,
					.fogmap_scale            = actual_fogmap_scale,
				});
			}
		}
		else 
		{
			ui_set_next_id(bake_cancel_button);
			if (ui_button(S("Cancel")))
			{
				bake_cancel(map->lightmap_state);

				for (size_t poly_index = 0; poly_index < map->poly_count; poly_index++)
				{
					map_poly_t *poly = &map->polys[poly_index];
					if (RESOURCE_HANDLE_VALID(poly->lightmap))
					{
						render->destroy_texture(poly->lightmap);
                        NULLIFY_HANDLE(poly->lightmap);
					}
				}

				render->destroy_texture(map->fogmap);
                NULLIFY_HANDLE(map->fogmap);

				map->lightmap_state = NULL;
			}
		}
	}

	if (map->lightmap_state && map->lightmap_state->finalized)
	{
		lum_bake_state_t *state = map->lightmap_state;

		double time_elapsed = state->final_bake_time;
		int minutes = (int)floor(time_elapsed / 60.0);
		int seconds = (int)floor(time_elapsed - 60.0*minutes);
		int microseconds = (int)floor(1000.0*(time_elapsed - 60.0*minutes - seconds));

		ui_header(S("Bake Results"));

		UI_SCALAR(UI_SCALAR_TEXT_ALIGN_X, 0.0f)
		{
			ui_label(Sf("Total Bake Time:  %02u:%02u:%03u", minutes, seconds, microseconds));
			ui_label(Sf("Fogmap Resolution: %u %u %u", map->fogmap_w, map->fogmap_h, map->fogmap_d));
		}

		if (ui_button(S("Clear Lightmaps")))
		{
			for (size_t poly_index = 0; poly_index < map->poly_count; poly_index++)
			{
				map_poly_t *poly = &map->polys[poly_index];
				if (RESOURCE_HANDLE_VALID(poly->lightmap))
				{
					render->destroy_texture(poly->lightmap);
					NULLIFY_HANDLE(poly->lightmap);
				}
			}

			render->destroy_texture(map->fogmap);
			NULLIFY_HANDLE(map->fogmap);

			release_bake_state(map->lightmap_state);
			map->lightmap_state = NULL;
		}
	}

	if (map->lightmap_state)
	{
		lum_bake_state_t *state = map->lightmap_state;

		if (state->finalized)
		{
			ui_header(S("Lightmap Debugger"));

			ui_checkbox(S("Enabled"), &lm_editor->debug_lightmaps);
			ui_checkbox(S("Show Direct Light Rays"), &lm_editor->show_direct_light_rays);
			ui_checkbox(S("Show Indirect Light Rays"), &lm_editor->show_indirect_light_rays);
			ui_checkbox(S("Fullbright Rays"), &lm_editor->fullbright_rays);
			ui_checkbox(S("No Ray Depth Test"), &lm_editor->no_ray_depth_test);

			ui_slider_int(S("Min Recursion Level"), &lm_editor->min_display_recursion, 0, 16);
			ui_slider_int(S("Max Recursion Level"), &lm_editor->max_display_recursion, 0, 16);

			if (lm_editor->selected_poly)
			{
				map_poly_t *poly = lm_editor->selected_poly;

				r_texture_desc_t desc;
				render->describe_texture(poly->lightmap, &desc);

				ui_push_scalar(UI_SCALAR_TEXT_ALIGN_X, 0.0f);

				ui_label(Sf("Resolution: %u x %u", desc.w, desc.h));
				if (lm_editor->pixel_selection_active)
				{
					ui_label(Sf("Selected Pixel Region: (%d, %d) (%d, %d)", 
								lm_editor->selected_pixels.min.x,
								lm_editor->selected_pixels.min.y,
								lm_editor->selected_pixels.max.x,
								lm_editor->selected_pixels.max.y));

					if (ui_button(S("Clear Selection")))
					{
						lm_editor->pixel_selection_active = false;
						lm_editor->selected_pixels = (rect2i_t){ 0 };
					}
				}
				else
				{
					// TODO: add spacers
					ui_label(S(""));
					ui_label(S(""));
				}

				ui_pop_scalar(UI_SCALAR_TEXT_ALIGN_X);

#if 0
                // TODO: Reimplement with UI draw commands
				float aspect = (float)desc.h / (float)desc.w;

				rect2_t *layout = ui_layout_rect();

				float width = rect2_width(*layout);

				rect2_t image_rect = rect2_cut_top(layout, width*aspect);

				r_immediate_texture(poly->lightmap);
				r_immediate_rect2_filled(image_rect, make_v4(1, 1, 1, 1));
				r_immediate_flush();
#endif

#if 0
				ui_interaction_t interaction = ui_interaction_from_box(image_viewer);
				if (interaction.hovering || lm_editor->pixel_selection_active)
				{
					v2_t rel_press_p = sub(interaction.press_p, image_viewer->rect.min);
					v2_t rel_mouse_p = sub(interaction.mouse_p, image_viewer->rect.min);

					v2_t rect_dim   = rect2_get_dim(image_viewer->rect);
					v2_t pixel_size = div(rect_dim, make_v2((float)desc.w, (float)desc.h));

					UI_Parent(image_viewer)
					{
						ui_box_t *selection_highlight = ui_box(S("selection highlight"), UI_DRAW_OUTLINE);
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
#endif
			}
		}
		else
		{
			float progress = bake_progress(map->lightmap_state);

			ui_progress_bar(Sf("bake progress: %u / %u (%.02f%%)", map->lightmap_state->jobs_completed, map->lightmap_state->job_count, 100.0f*progress), progress);

			hires_time_t current_time = os_hires_time();
			double time_elapsed = os_seconds_elapsed(map->lightmap_state->start_time, current_time);
			int minutes = (int)floor(time_elapsed / 60.0);
			int seconds = (int)floor(time_elapsed - 60.0*minutes);

			ui_label(Sf("time elapsed:  %02u:%02u", minutes, seconds));
		}
	}
}

static void render_lm_editor(r_context_t *rc)
{
    world_t *world = g_world;
    player_t *player = world->player;
    map_t    *map    = world->map;

    map_entity_t *worldspawn = map->worldspawn;

    v3_t  sun_color      = v3_normalize(v3_from_key(map, worldspawn, S("sun_color")));
    float sun_brightness = float_from_key(map, worldspawn, S("sun_brightness"));

    sun_color = mul(sun_brightness, sun_color);

    v3_t ambient_color = v3_from_key(map, worldspawn, S("ambient_color"));
    ambient_color = mul(1.0f / 255.0f, ambient_color);

    lightmap_editor_state_t *lm_editor = &editor.lm_editor;

    if (lm_editor->debug_lightmaps)
    {
        r_push_command_identifier(rc, S("lightmap debug"));

        r_immediate_t *imm = r_immediate_begin(rc);
        imm->topology   = R_TOPOLOGY_LINELIST;
        imm->blend_mode = R_BLEND_ADDITIVE;
		imm->use_depth  = lm_editor->no_ray_depth_test;

        if (g_cursor_locked)
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
                    push_poly_wireframe(rc, map, intersect.poly, COLORF_RED);
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

            push_poly_wireframe(rc, map, lm_editor->selected_poly, make_v4(0.0f, 0.0f, 0.0f, 0.75f));

            r_immediate_arrow(rc, plane->lm_origin, add(plane->lm_origin, mul(scale_x, plane->lm_s)), make_v4(0.5f, 0.0f, 0.0f, 1.0f));
            r_immediate_arrow(rc, plane->lm_origin, add(plane->lm_origin, mul(scale_y, plane->lm_t)), make_v4(0.0f, 0.5f, 0.0f, 1.0f));

            v3_t square_v0 = add(plane->lm_origin, mul(scale_x, plane->lm_s));
            v3_t square_v1 = add(plane->lm_origin, mul(scale_y, plane->lm_t));
            v3_t square_v2 = v3_add3(plane->lm_origin, mul(scale_x, plane->lm_s), mul(scale_y, plane->lm_t));

            r_immediate_line(rc, square_v0, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));
            r_immediate_line(rc, square_v1, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));

            if (lm_editor->pixel_selection_active)
            {
                r_texture_desc_t desc;
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

                r_immediate_line(rc, pixel_v0, pixel_v1, COLORF_RED);
                r_immediate_line(rc, pixel_v0, pixel_v2, COLORF_RED);
                r_immediate_line(rc, pixel_v2, pixel_v3, COLORF_RED);
                r_immediate_line(rc, pixel_v1, pixel_v3, COLORF_RED);
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
										// r_immediate_line(vertex->o, add(vertex->o, mul(sample->shadow_ray_t, sample->d)), COLORF_RED);
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
											r_immediate_arrow(point_light->p, vertex->o, 
														 make_v4(color.x, color.y, color.z, 1.0f));
										}
#endif

										if (!vertex->next || vertex_index + 2 == (int)path->vertex_count)
										{
											r_immediate_line(rc, vertex->o, point_light->p, 
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
										r_immediate_arrow_gradient(rc, next_vertex->o, vertex->o, end_color, start_color);
									}
									else
									{
										r_immediate_line_gradient(rc, vertex->o, next_vertex->o, start_color, end_color);
									}
								}
							}

							vertex_index += 1;
						}
					}
				}
			}
		}

        r_immediate_end(rc, imm);
        r_pop_command_identifier(rc);
    }
}

fn_local void fullscreen_show_timings(void)
{
    UI_SCALAR(UI_SCALAR_TEXT_ALIGN_X, 0.5f)
    {
        r_timings_t timings;
        render->get_timings(&timings);

        float total = 0.0f;
        for (int i = 1; i < RENDER_TS_COUNT; i++)
        {
            ui_label(Sf("%s: %.02fms", r_timestamp_names[i], 1000.0*timings.dt[i]));
            total += timings.dt[i];
        }

        ui_label(Sf("total: %.02fms", 1000.0*total));
    }
}

fn_local void ui_demo_proc(ui_window_t *window)
{
	if (window->hovered)
	{
		ui_tooltip(S("Window for testing out various UI features."));
	}

	ui_demo_panel_t *demo = &editor.ui_demo;

	static bool check_me = false;
	ui_hover_tooltip(S("This checkbox does nothing! GOOD DAY SIR!!"));
	ui_checkbox(S("Checkbox That Does Nothing"), &check_me);

	ui_slider    (S("Float Slider"), &demo->slider_f32, -1.0f, 1.0f);
	ui_slider_int(S("Int Slider"), &demo->slider_i32, 0, 8);

	static int font_size = 18;
	if (ui_slider_int(S("UI Font Size"), &font_size, 8, 32))
	{
		ui_set_font_height((float)font_size);
	}

	ui_hover_tooltip(S("Size of the inner margin for windows"));
	ui_slider(S("UI Window Margin"), &ui.style.base_scalars[UI_SCALAR_WINDOW_MARGIN], 0.0f, 8.0f);
	ui_hover_tooltip(S("Size of the margin between widgets"));
	ui_slider(S("UI Widget Margin"), &ui.style.base_scalars[UI_SCALAR_WIDGET_MARGIN], 0.0f, 8.0f);
	ui_hover_tooltip(S("Size of the margin between widgets and their text contents (e.g. a button and its label)"));
	ui_slider(S("UI Text Margin"), &ui.style.base_scalars[UI_SCALAR_TEXT_MARGIN], 0.0f, 8.0f);
	ui_hover_tooltip(S("Roundedness of UI elements in pixel radius"));
	ui_slider(S("UI Roundedness"), &ui.style.base_scalars[UI_SCALAR_ROUNDEDNESS], 0.0f, 12.0f);
	ui_hover_tooltip(S("Spring stiffness coefficient for animations"));
	ui_slider(S("UI Animation Stiffness"), &ui.style.base_scalars[UI_SCALAR_ANIMATION_STIFFNESS], 1.0f, 1024.0f);
	ui_hover_tooltip(S("Spring dampen coefficient for animations"));
	ui_slider(S("UI Animation Dampen"), &ui.style.base_scalars[UI_SCALAR_ANIMATION_DAMPEN], 1.0f, 128.0f);

	demo->edit_buffer.data     = demo->edit_buffer_storage;
	demo->edit_buffer.capacity = ARRAY_COUNT(demo->edit_buffer_storage);
	ui_hover_tooltip(S("Text edit demo box"));
	ui_text_edit(S("Text Edit"), &demo->edit_buffer);

	if (ui_button(S("Hot Reload Dog")))
	{
		reload_asset(asset_hash_from_string(S("gamedata/textures/dog.png")));
	}

	if (ui_button(S("Toggle Music")))
	{
		static bool       is_playing = false;
		static mixer_id_t music_handle;

		if (is_playing)
		{
			stop_sound(music_handle);
			is_playing = false;
		}
		else
		{
			music_handle = play_sound(&(play_sound_t){
				.waveform     = get_waveform_from_string(S("gamedata/audio/test.wav")),
				.volume       = 1.0f,
				.p            = make_v3(0, 0, 0),
				.min_distance = 100000.0f,
				.flags        = PLAY_SOUND_SPATIAL|PLAY_SOUND_FORCE_MONO|PLAY_SOUND_LOOPING,
			});
			is_playing = true;
		}
	}

	ui_slider(S("Reverb Amount"), &mixer.reverb_amount, 0.0f, 1.0f);
	ui_slider(S("Reverb Feedback"), &mixer.reverb_feedback, -1.0f, 1.0f);
	ui_slider_int(S("Delay Time"), &mixer.reverb_delay_time, 500, 2000);
	ui_slider(S("Stereo Spread"), &mixer.reverb_stereo_spread, 0.0f, 2.0f);
	ui_slider(S("Diffusion Angle"), &mixer.reverb_diffusion_angle, 0.0f, 90.0f);

	ui_slider(S("Filter Test"), &mixer.filter_test, -1.0f, 1.0f);

	static size_t selection_index = 0;

	static string_t options[] = {
		Sc("Option A"),
		Sc("Option B"),
		Sc("Option C"),
	};

	ui_combo_box(S("Options"), &selection_index, ARRAY_COUNT(options), options);

	ui_button(S("Useless button #1"));
	ui_button(S("Useless button #2"));
	ui_button(S("Useless button #3"));
	ui_button(S("Useless button #4"));
}

fn_local void fullscreen_update_and_render_top_editor_bar(void)
{
    rect2_t bar = rect2_cut_top(editor.fullscreen_layout, 32.0f);

    rect2_t collision_bar = bar;
    collision_bar.min.y -= 64.0f;

	if (editor.bar_openness > 0.0001f)
		collision_bar.min.y -= 128.0f;

    bool mouse_hover = ui_mouse_in_rect(collision_bar);
	mouse_hover |= ui.input.mouse_p.y >= collision_bar.max.y;
	mouse_hover |= editor.pin_bar;

    editor.bar_openness = ui_interpolate_f32(ui_id_pointer(&editor.bar_openness), mouse_hover ? 1.0f : 0.0f);

    if (editor.bar_openness > 0.0001f)
    {
        float height = rect2_height(bar);

        bar = rect2_add_offset(bar, make_v2(0.0f, (1.0f - editor.bar_openness)*height));

#if 0
        // TODO: Replace with UI draw calls
        r_immediate_rect2_filled(bar, ui_color(UI_COLOR_WINDOW_BACKGROUND));
        r_immediate_flush();
#endif

        rect2_t inner_bar = rect2_shrink(bar, ui_scalar(UI_SCALAR_WIDGET_MARGIN));

		UI_SCALAR(UI_SCALAR_HOVER_LIFT, 0.0f)
        UI_PANEL(inner_bar)
        {
			ui_set_layout_direction(RECT2_CUT_LEFT);

			ui_checkbox(S("Pin"), &editor.pin_bar);

			ui_set_layout_direction(RECT2_CUT_RIGHT);

            typedef struct menu_t
            {
                string_t name;
                bool *toggle;
				ui_window_t *window;
            } menu_t;

            menu_t menus[] = {
                {
                    .name = S("Timings (F1)"),
                    .toggle = &editor.show_timings,
                },
                {
                    .name = S("Lightmap Editor (F2)"),
                    .window = &editor.lm_editor.window,
                },
                {
                    .name = S("Convex Hull Tester (F3)"),
                    .window = &editor.convex_hull_debugger.window,
                },
                {
                    .name = S("UI Demo (F4)"),
                    .window = &editor.ui_demo.window,
                },
            };

            for (int64_t i = ARRAY_COUNT(menus) - 1; i >= 0; i--)
            {
                menu_t *menu = &menus[i];
                
                bool active = menu->toggle ? *menu->toggle : menu->window->open;

                if (active) ui_push_color(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_BUTTON_ACTIVE));

                if (ui_button(menu->name))
                {
					if (menu->toggle) *menu->toggle = !*menu->toggle;
					if (menu->window)
					{
						ui_toggle_window_openness(menu->window);
					}
                }

                if (active) ui_pop_color(UI_COLOR_BUTTON_IDLE);
            }

            ui_label(S("  Menus: "));

            ui_label(Sf("Mouse Position: %.02f x %.02f", ui.input.mouse_p.x, ui.input.mouse_p.y));
			ui_label(Sf("Active UI Animation Count: %zu", ui.style.animation_state.count));
			ui_label(Sf("Delta Time: %.02fms", 1000.0f*ui.input.dt));
			ui_label(Sf("UI Hover Time: %.02f", ui.hover_time_seconds));
			ui_label(Sf("UI Rect Count: %zu", ui.last_frame_ui_rect_count));
#if DREAM_SLOW
			ui_label(Sf("Hot ID: '%s'", ui.hot.name));
			ui_label(Sf("Active ID: '%s'", ui.active.name));
			ui_label(Sf("Hovered Panel: '%s'", ui.hovered_panel.name));
			ui_label(Sf("Hovered Widget: '%s'", ui.hovered_widget.name));
#endif
        }
    }
}

void update_and_render_in_game_editor(r_context_t *rc, r_view_index_t game_view)
{
    r_push_command_identifier(rc, S("update_and_render_in_game_editor"));

    r_push_view      (rc, rc->screenspace);
    r_push_view_layer(rc, R_VIEW_LAYER_UI);
    r_push_layer     (rc, R_SCREEN_LAYER_UI);

	if (!editor.initialized)
	{
		editor.initialized = true;

		ui_add_window(&editor.lm_editor.window);
		editor.lm_editor.window.rect = rect2_from_min_dim(make_v2(32, 32), make_v2(512, 512));

		ui_add_window(&editor.convex_hull_debugger.window);
		editor.convex_hull_debugger.window.rect = rect2_from_min_dim(make_v2(64, 64), make_v2(512, 512));

		ui_add_window(&editor.ui_demo.window);
		editor.ui_demo.window.rect = rect2_from_min_dim(make_v2(96, 96), make_v2(512, 512));
	}

    if (ui_button_pressed(BUTTON_F1))
	{
        editor.show_timings = !editor.show_timings;
	}

    if (ui_button_pressed(BUTTON_F2))
	{
		ui_toggle_window_openness(&editor.lm_editor.window);
	}

    if (ui_button_pressed(BUTTON_F3))
	{
		ui_toggle_window_openness(&editor.convex_hull_debugger.window);
	}

    if (ui_button_pressed(BUTTON_F4))
	{
		ui_toggle_window_openness(&editor.ui_demo.window);
	}

	if (&editor.lm_editor.window.open)
		render_lm_editor(rc);

	if (&editor.convex_hull_debugger.window.open)
		convex_hull_debugger_update_and_render(&editor.convex_hull_debugger, rc, game_view);

    int res_x, res_y;
    render->get_resolution(&res_x, &res_y);

    rect2_t fullscreen_rect = {
        .min = { 0, 0 },
        .max = { (float)res_x, (float)res_y },
    };

    UI_SCALAR(UI_SCALAR_WIDGET_MARGIN, 0.0f)
    ui_panel_begin(fullscreen_rect);
	{
		editor.fullscreen_layout = ui_layout_rect();

		fullscreen_update_and_render_top_editor_bar();

		if (editor.show_timings)
			fullscreen_show_timings();
	}

#if 0
	rect2_t r  = rect2_center_radius(ui.input.mouse_p, make_v2(1, 1));
	rect2_t r2 = rect2_from_min_dim(ui.input.mouse_p, make_v2(512, 1));
	ui_draw_rect(rect2_extend(r2, 1.0f), make_v4(0, 0, 0, 1));
	ui_draw_rect(rect2_extend(r, 1.0f), make_v4(0, 0, 0, 1));
	ui_draw_rect(r2, make_v4(1, 1, 1, 1));
	ui_draw_rect(r , make_v4(1, 1, 1, 1));
	ui_draw_text(&ui.style.font, r.min, S("The Quick Brown Fox Jumped Over The Lazy Dog"));
#endif

	ui_panel_end();

	ui_process_windows();

    r_pop_layer     (rc);
    r_pop_view_layer(rc);
    r_pop_view      (rc);

    r_pop_command_identifier(rc);
}
