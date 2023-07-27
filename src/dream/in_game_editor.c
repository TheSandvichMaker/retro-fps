#include "core/core.h"

#include "dream/render.h"
#include "dream/render_helpers.h"
#include "dream/light_baker.h"
#include "dream/in_game_editor.h"
#include "dream/game.h"
#include "dream/map.h"
#include "dream/debug_ui.h"
#include "dream/intersect.h"
#include "dream/asset.h"
#include "dream/audio.h"
#include "dream/mesh.h"

static void push_poly_wireframe(map_t *map, map_poly_t *poly, v4_t color)
{
    uint16_t *indices   = map->indices          + poly->first_index;
    v3_t     *positions = map->vertex.positions + poly->first_vertex;

    for (size_t triangle_index = 0; triangle_index < poly->index_count / 3; triangle_index++)
    {
        v3_t a = positions[indices[3*triangle_index + 0]];
        v3_t b = positions[indices[3*triangle_index + 1]];
        v3_t c = positions[indices[3*triangle_index + 2]];

        r_immediate_line(a, b, color);
        r_immediate_line(a, c, color);
        r_immediate_line(b, c, color);
    }
}

static void push_brush_wireframe(map_t *map, map_brush_t *brush, v4_t color)
{
    for (size_t poly_index = 0; poly_index < brush->plane_poly_count; poly_index++)
    {
        map_poly_t *poly = &map->polys[brush->first_plane_poly + poly_index];
        push_poly_wireframe(map, poly, color);
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

typedef struct ui_demo_panel_t
{
	ui_window_t window;
	float slider_f32;
	int   slider_i32;
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

typedef struct hull_test_panel_t
{
	ui_window_t window;

	bool initialized;

	int random_seed;
	int point_count;
	bool automatically_recalculate_hull;

	arena_t         mesh_arena;
	triangle_mesh_t mesh;
	hull_debug_t    debug;

	bool   show_points;
	bool   show_processed_edge;
	bool   show_non_processed_edges;
	bool   show_new_triangle;
	bool   show_triangles;
	bool   show_duplicate_triangles;
	bool   show_wireframe;
	int    current_step_index;
	int    test_tetrahedron_index;

	bool   brute_force_test;
	int    brute_force_min_point_count;
	int    brute_force_max_point_count;
	int    brute_force_iterations_per_point_count;
	float  brute_force_triggered_success_timer;
	float  brute_force_triggered_error_timer;
} hull_test_panel_t;

typedef stack_t(editor_kind_t, EDITOR_COUNT) editor_stack_t;

typedef struct editor_state_t
{
	bool initialized;

    rect2_t *fullscreen_layout;
    float bar_openness;

    bool show_timings;

    bool lightmap_editor_enabled;
    lightmap_editor_state_t lm_editor;

	bool hull_test_panel_enabled;
	hull_test_panel_t hull_test;

	ui_demo_panel_t ui_demo;

	// TODO: bit silly
	editor_stack_t next_window_order;
	editor_stack_t window_order;
} editor_state_t;

static editor_state_t editor = {
    .show_timings = false,

    .lm_editor = {
		.window = {
			.name = Sc("Lightmap Editor"),
		},

		.min_display_recursion = 0,
		.max_display_recursion = 10,
    },

	.hull_test = {
		.window = {
			.name = Sc("Convex Hull Debugger"),
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
};

static void bring_editor_to_front(editor_kind_t kind)
{
	stack_erase(editor.next_window_order, kind);
	stack_push_back(editor.next_window_order, kind); // in front = back of the queue! because it's back-to-front!
}

static void send_editor_to_back(editor_kind_t kind)
{
	stack_erase(editor.next_window_order, kind);
	stack_push_front(editor.next_window_order, kind); // to back = front of the queue! because it's back-to-front!
}

static void generate_new_random_convex_hull(size_t point_count, random_series_t *r)
{
	hull_test_panel_t *hull_test = &editor.hull_test;

	triangle_mesh_t *mesh  = &hull_test->mesh;
	hull_debug_t    *debug = &hull_test->debug;

	m_scoped(temp)
	{
		v3_t *random_points = m_alloc_array_nozero(temp, point_count, v3_t);

		for (size_t i = 0; i < point_count; i++)
		{
			random_points[i] = add(make_v3(512.0f, 512.0f, 512.0f), mul(128.0f, random_in_unit_cube(r)));
		}

		bool at_final_step = false;
		if (debug->step_count > 0 &&
			hull_test->current_step_index >= (int)debug->step_count)
		{
			at_final_step = true;
		}

		m_reset(&hull_test->mesh_arena);

		*mesh = calculate_convex_hull_debug(&hull_test->mesh_arena, point_count, random_points, debug);
		convex_hull_do_extended_diagnostics(mesh, debug);

		if (at_final_step)
		{
			hull_test->current_step_index = (int)debug->step_count;
		}
	}
}

static void hull_render_debug_triangles(hull_debug_step_t *step, bool final_step, bool render_degenerate, bool render_wireframe)
{
	hull_test_panel_t        *hull_test = &editor.hull_test;
	hull_debug_t             *debug   = &hull_test->debug;
	hull_diagnostic_result_t *diag    = debug->diagnostics;

	m_scoped(temp)
	{
		float alpha = 0.6f;

		v4_t base_color         = render_degenerate ? make_v4(1.0f, 0.25f, 0.0f, alpha) : make_v4(0.7f, 0.9f, 0.5f, alpha);
		v4_t new_triangle_color = render_degenerate ? make_v4(1.0f, 0.65f, 0.0f, alpha) : make_v4(1.0f, 1.0f, 0.5f, alpha);

		int triangle_index = 0;
		for (hull_debug_triangle_t *triangle = step->first_triangle;
			 triangle;
			 triangle = triangle->next, triangle_index++)
		{
			bool show = false;

			if (hull_test->show_triangles)                                 show = true;
			if (hull_test->show_new_triangle && triangle->added_this_step) show = true;
			if (!show) continue;

			if (diag->triangle_is_degenerate[triangle_index] != render_degenerate) 
				continue;

			v3_t a = triangle->t.a;
			v3_t b = triangle->t.b;
			v3_t c = triangle->t.c;

			v4_t color = base_color;

			if (!final_step && hull_test->show_new_triangle && triangle->added_this_step)
				color = new_triangle_color;

			if (hull_test->show_duplicate_triangles &&
				diag->duplicate_triangle_index[triangle_index] != -1 
				&& diag->duplicate_triangle_index[triangle_index] < triangle_index)
			{
				v3_t normal = triangle_normal(a, b, c);
				v3_t displacement = mul(normal, 4.0f);
				a = add(a, displacement);
				b = add(b, displacement);
				c = add(c, displacement);
				color = v4_lerps(color, make_v4(1.0f, 0.5f, 0.0f, render_wireframe ? 0.6f : 0.1f), 0.5f);
			}

			if (render_wireframe)
			{
				r_immediate_line(a, b, color);
				r_immediate_line(a, c, color);
				r_immediate_line(b, c, color);
			}
			else
			{
				triangle_t t = { a, b, c };
				r_immediate_triangle(t, color);
			}
		}
	}
}

static void update_and_render_convex_hull_test_panel(void)
{
	hull_test_panel_t *hull_test = &editor.hull_test;

	if (!hull_test->window.open)
		return;

	triangle_mesh_t *mesh  = &hull_test->mesh;
	hull_debug_t    *debug = &hull_test->debug;

	bool degenerate_hull = false;

	int uncontained_point_count   = 0;
	int degenerate_triangle_count = 0;
	int duplicate_triangle_count  = 0;
	int no_area_triangle_count    = 0;

	bool *point_fully_contained    = NULL;
	bool *triangle_is_degenerate   = NULL;
	int  *duplicate_triangle_index = NULL;
	bool *triangle_has_no_area     = NULL;

	if (debug->diagnostics)
	{
		hull_diagnostic_result_t *diagnostics = debug->diagnostics;

		degenerate_hull           = diagnostics->degenerate_hull;
		uncontained_point_count   = diagnostics->uncontained_point_count;
		degenerate_triangle_count = diagnostics->degenerate_triangle_count;
		duplicate_triangle_count  = diagnostics->duplicate_triangle_count; 
		no_area_triangle_count    = diagnostics->no_area_triangle_count;
		point_fully_contained     = diagnostics->point_fully_contained;
		triangle_is_degenerate    = diagnostics->triangle_is_degenerate;
		duplicate_triangle_index  = diagnostics->duplicate_triangle_index;
		triangle_has_no_area      = diagnostics->triangle_has_no_area;
	}

	if (debug->step_count > 0)
	{
		if (hull_test->show_points)
		{
			r_immediate_topology(R_TOPOLOGY_LINELIST);
			r_immediate_use_depth(false);

			for (size_t i = 0; i < debug->initial_points_count; i++)
			{
				v3_t p = debug->initial_points[i];

				v4_t color = ((int)i == hull_test->test_tetrahedron_index ? COLORF_ORANGE : COLORF_RED);

				if (point_fully_contained[i])
				{
					color.xyz = mul(color.xyz, 0.75f);
				}

				r_immediate_rect3_outline(rect3_center_radius(p, make_v3(1, 1, 1)), color);
			}

			r_immediate_flush();
		}

		//

		r_immediate_topology (R_TOPOLOGY_TRIANGLELIST);
		r_immediate_shader   (R_SHADER_DEBUG_LIGHTING);
		r_immediate_use_depth(true);
		r_immediate_cull_mode(R_CULL_BACK);

		int step_index = 0;
		for (hull_debug_step_t *step = hull_test->debug.first_step;
			 step;
			 step = step->next)
		{
			if (step_index == MIN(hull_test->current_step_index, (int)debug->step_count - 1))
			{
				bool final_step = (hull_test->current_step_index == (int)debug->step_count);

				hull_render_debug_triangles(step, final_step, false, false);
				hull_render_debug_triangles(step, final_step, true, false);
			}

			step_index++;
		}

		r_immediate_flush();

		//

		r_immediate_topology (R_TOPOLOGY_LINELIST);
		r_immediate_shader   (R_SHADER_FLAT);
		r_immediate_use_depth(true);

		step_index = 0;
		for (hull_debug_step_t *step = hull_test->debug.first_step;
			 step;
			 step = step->next)
		{
			if (step_index == MIN(hull_test->current_step_index, (int)debug->step_count - 1))
			{
				bool final_step = (hull_test->current_step_index == (int)debug->step_count);

				if (hull_test->show_wireframe)
				{
					hull_render_debug_triangles(step, final_step, false, true);
					hull_render_debug_triangles(step, final_step, true, true);
				}

				if (!final_step)
				{
					for (hull_debug_edge_t *edge = step->first_edge;
						 edge;
						 edge = edge->next)
					{
						bool show = false;

						if (hull_test->show_processed_edge      &&  edge->processed_this_step) show = true;
						if (hull_test->show_non_processed_edges && !edge->processed_this_step) show = true;
						if (!show) continue;

						v3_t a = edge->e.a;
						v3_t b = edge->e.b;

						r_immediate_line(a, b, (edge->processed_this_step ? COLORF_WHITE : COLORF_BLUE));
					}
				}
			}

			step_index++;
		}

		r_immediate_flush();
	}

	UI_WINDOW(&hull_test->window)
	{
		bool changed = false;
		changed |= ui_slider_int(S("Random Seed"), &hull_test->random_seed, 1, 128);
		changed |= ui_slider_int(S("Point Count"), &hull_test->point_count, 8, 256);

		ui_checkbox(S("Automatically Recalculate Hull"), &hull_test->automatically_recalculate_hull);

		bool should_recalculate = ui_button(S("Calculate Random Convex Hull"));
		if (hull_test->automatically_recalculate_hull && changed)               should_recalculate = true;
		if (hull_test->automatically_recalculate_hull && !hull_test->initialized) should_recalculate = true;

		if (should_recalculate)
		{
			random_series_t r = { (uint32_t)hull_test->random_seed };
			generate_new_random_convex_hull((size_t)hull_test->point_count, &r);
		}

		if (mesh->triangle_count > 0)
		{
			if (ui_button(S("Delete Convex Hull Data")))
			{
				m_release(&hull_test->mesh_arena);
				m_release(&hull_test->debug.arena);
				zero_struct(mesh);
				zero_struct(debug);
			}
		}

		ui_label(S("Brute Force Tester"));
		ui_slider_int(S("Min Points"), &hull_test->brute_force_min_point_count, 4, 256);
		ui_slider_int(S("Max Points"), &hull_test->brute_force_max_point_count, 4, 256);
		ui_slider_int(S("Iterations Per Count"), &hull_test->brute_force_iterations_per_point_count, 1, 4096);

		if (ui_button(S("Run Brute Force Test")))
		{
			random_series_t r = { (uint32_t)hull_test->random_seed };

			hull_test->brute_force_triggered_success_timer = 0.0f;
			hull_test->brute_force_triggered_error_timer   = 0.0f;

			for (int point_count = hull_test->brute_force_min_point_count;
				 point_count <= hull_test->brute_force_max_point_count;
				 point_count++)
			{
				generate_new_random_convex_hull((size_t)point_count, &r);

				if (ALWAYS(debug->diagnostics))
				{
					hull_diagnostic_result_t *diagnostics = debug->diagnostics;
					if (diagnostics->degenerate_hull)
					{
						hull_test->brute_force_triggered_error_timer = 10.0f;
						break;
					}
				}
			}

			if (hull_test->brute_force_triggered_error_timer <= 0.0f)
			{
				hull_test->brute_force_triggered_success_timer = 5.0f;
			}
		}

		if (hull_test->brute_force_triggered_success_timer > 0.0f)
		{
			UI_COLOR(UI_COLOR_TEXT, make_v4(0.5f, 1.0f, 0.2f, 1.0f))
			ui_label(S("Brute force test passed."));

			hull_test->brute_force_triggered_success_timer -= ui.dt;
		}

		if (hull_test->brute_force_triggered_error_timer > 0.0f)
		{
			UI_COLOR(UI_COLOR_TEXT, COLORF_RED)
			ui_label(S("BRUTE FORCE TEST FOUND A DEGENERATE HULL!?!?!"));

			hull_test->brute_force_triggered_error_timer -= ui.dt;
		}

		if (degenerate_hull)
		{
			UI_COLOR(UI_COLOR_TEXT, COLORF_RED)
			ui_label(S("!! DEGENERATE CONVEX HULL !!"));
			if (triangle_is_degenerate[0])
			{
				UI_COLOR(UI_COLOR_TEXT, COLORF_RED)
				ui_label(S("THE FIRST TRIANGLE IS DEGENERATE, THAT'S NO GOOD"));
			}
			ui_label(Sf("Degenerate Triangle Count: %d", degenerate_triangle_count));
		}

		if (duplicate_triangle_count > 0)
		{
			UI_COLOR(UI_COLOR_TEXT, COLORF_ORANGE)
			ui_label(Sf("There are %d duplicate triangles!", duplicate_triangle_count));
		}

		if (no_area_triangle_count > 0)
		{
			UI_COLOR(UI_COLOR_TEXT, COLORF_ORANGE)
			ui_label(Sf("There are %d triangles with (nearly) 0 area!", no_area_triangle_count));
			if (triangle_is_degenerate[0])
			{
				UI_COLOR(UI_COLOR_TEXT, COLORF_ORANGE)
				ui_label(S("The first triangle has no area, which seems bad"));
			}
		}

		if (debug->step_count > 0)
		{
			ui_checkbox(S("Show initial points"), &hull_test->show_points);
			ui_checkbox(S("Show processed edge"), &hull_test->show_processed_edge);
			ui_checkbox(S("Show non-processed edges"), &hull_test->show_non_processed_edges);
			ui_checkbox(S("Show new triangle"), &hull_test->show_new_triangle);
			ui_checkbox(S("Show all triangles"), &hull_test->show_triangles);
			ui_checkbox(S("Show duplicate triangles"), &hull_test->show_duplicate_triangles);
			ui_checkbox(S("Show wireframe"), &hull_test->show_wireframe);
			ui_slider_int(S("Step"), &hull_test->current_step_index, 0, (int)(hull_test->debug.step_count));

			if (hull_test->current_step_index > (int)hull_test->debug.step_count)
			    hull_test->current_step_index = (int)hull_test->debug.step_count;

			int step_index = 0;
			hull_debug_step_t *step = debug->first_step;
			for (;
				 step;
				 step = step->next)
			{
				if (step_index == MIN(hull_test->current_step_index, (int)debug->step_count - 1))
				{
					break;
				}
				step_index++;
			}

			if (ALWAYS(step))
			{
				ui_label(Sf("edge count: %zu", step->edge_count));
				ui_label(Sf("triangle count: %zu", step->triangle_count));

				ui_slider_int(S("Test Tetrahedron Index"), &hull_test->test_tetrahedron_index, -1, (int)(debug->initial_points_count) - 1);

				if (hull_test->test_tetrahedron_index > (int)debug->initial_points_count - 1)
				    hull_test->test_tetrahedron_index = (int)debug->initial_points_count - 1;

				if (hull_test->test_tetrahedron_index >= 0)
				{
					triangle_t t = step->first_triangle->t;
					ASSERT(step->first_triangle->added_this_step);

					v3_t p = debug->initial_points[hull_test->test_tetrahedron_index];

					float volume = tetrahedron_signed_volume(t.a, t.b, t.c, p);
					ui_label(Sf("Tetrahedron Volume: %f", volume));

					float area = triangle_area_sq(t.a, t.b, p);
					ui_label(Sf("Squared Triangle Area: %f", area));
				}
			}
		}
	}

	if (ui_is_active(hull_test->window.id))
	{
		bring_editor_to_front(EDITOR_CONVEX_HULL);
	}

	hull_test->initialized = true;
}

static void update_and_render_lightmap_editor(void)
{
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

    lightmap_editor_state_t *lm_editor = &editor.lm_editor;

	if (!lm_editor->window.open)
		return;

	UI_WINDOW(&lm_editor->window)
	{
        if (!map->lightmap_state || !map->lightmap_state->finalized)
        {
            ui_label(S("Bake Quality"));

            static int bake_preset              = 0;
            static int ray_count                = 2;
            static int ray_recursion            = 2;
            static int fog_light_sample_count   = 2;
            static int fogmap_scale_index       = 1;
            static bool use_dynamic_sun_shadows = true;

            static string_t preset_labels[] = { Sc("Crappy"), Sc("Acceptable"), Sc("Excessive") };
            if (ui_radio(S("preset"), &bake_preset, ARRAY_COUNT(preset_labels), preset_labels))
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

            ui_slider_int(S("rays per pixel"), &ray_count, 1, 32);
            ui_slider_int(S("max recursion"), &ray_recursion, 1, 8);
            ui_slider_int(S("fog light sample count"), &fog_light_sample_count, 1, 8);

            static int fogmap_scales[] = {
                32, 16, 8, 4,
            };

            static string_t fogmap_scale_labels[] = {
                Sc("32"), Sc("16"), Sc("8"), Sc("4"),
            };

            ui_radio(S("fogmap scale"), &fogmap_scale_index, ARRAY_COUNT(fogmap_scales), fogmap_scale_labels);

            int actual_fogmap_scale = fogmap_scales[fogmap_scale_index];

            ui_checkbox(S("dynamic sun shadows"), &use_dynamic_sun_shadows);

            if (!map->lightmap_state)
            {
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
            else if (ui_button(S("Cancel")))
            {
                bake_cancel(map->lightmap_state);

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

                map->lightmap_state = NULL;
            }
        }

		if (map->lightmap_state && map->lightmap_state->finalized)
		{
			lum_bake_state_t *state = map->lightmap_state;

            double time_elapsed = state->final_bake_time;
            int minutes = (int)floor(time_elapsed / 60.0);
            int seconds = (int)floor(time_elapsed - 60.0*minutes);
            int microseconds = (int)floor(1000.0*(time_elapsed - 60.0*minutes - seconds));

            UI_SCALAR(UI_SCALAR_TEXT_ALIGN_X, 0.0f)
            {
                ui_label(Sf("total bake time:  %02u:%02u:%03u", minutes, seconds, microseconds));
                ui_label(Sf("fogmap resolution: %u %u %u", map->fogmap_w, map->fogmap_h, map->fogmap_d));
            }

			if (ui_button(S("Clear Lightmaps")))
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
				ui_label(S(""));
				ui_label(S("Lightmap Debugger"));

				ui_checkbox(S("enabled"), &lm_editor->debug_lightmaps);
				ui_checkbox(S("show direct light rays"), &lm_editor->show_direct_light_rays);
				ui_checkbox(S("show indirect light rays"), &lm_editor->show_indirect_light_rays);
				ui_checkbox(S("fullbright rays"), &lm_editor->fullbright_rays);
				ui_checkbox(S("no ray depth test"), &lm_editor->no_ray_depth_test);

				ui_slider_int(S("min recursion level"), &lm_editor->min_display_recursion, 0, 16);
				ui_slider_int(S("max recursion level"), &lm_editor->max_display_recursion, 0, 16);

				if (lm_editor->selected_poly)
				{
					map_poly_t *poly = lm_editor->selected_poly;

					texture_desc_t desc;
					render->describe_texture(poly->lightmap, &desc);

                    ui_push_scalar(UI_SCALAR_TEXT_ALIGN_X, 0.0f);

					ui_label(Sf("resolution: %u x %u", desc.w, desc.h));
					if (lm_editor->pixel_selection_active)
					{
                        ui_label(Sf("selected pixel region: (%d, %d) (%d, %d)", 
                                    lm_editor->selected_pixels.min.x,
                                    lm_editor->selected_pixels.min.y,
                                    lm_editor->selected_pixels.max.x,
                                    lm_editor->selected_pixels.max.y));

						if (ui_button(S("clear selection")))
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

                    float aspect = (float)desc.h / (float)desc.w;

                    rect2_t *layout = ui_layout_rect();

                    float width = rect2_width(*layout);

                    rect2_t image_rect = ui_cut_top(layout, width*aspect);

                    UI_PANEL(image_rect)
                    {
                        r_immediate_texture(poly->lightmap);
                        r_immediate_rect2_filled(image_rect, make_v4(1, 1, 1, 1));
                        r_immediate_flush();
                    }

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

	if (ui_is_active(lm_editor->window.id))
	{
		bring_editor_to_front(EDITOR_LIGHTMAP);
	}

    if (lm_editor->debug_lightmaps)
    {
        r_command_identifier(S("lightmap debug"));

		r_immediate_topology(R_TOPOLOGY_LINELIST);
		r_immediate_use_depth(!lm_editor->no_ray_depth_test);
		//r_immediate_depth_bias(0.005f);
		r_immediate_blend_mode(R_BLEND_ADDITIVE);

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
                    push_poly_wireframe(map, intersect.poly, COLORF_RED);
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

            push_poly_wireframe(map, lm_editor->selected_poly, make_v4(0.0f, 0.0f, 0.0f, 0.75f));

            r_immediate_arrow(plane->lm_origin, add(plane->lm_origin, mul(scale_x, plane->lm_s)), make_v4(0.5f, 0.0f, 0.0f, 1.0f));
            r_immediate_arrow(plane->lm_origin, add(plane->lm_origin, mul(scale_y, plane->lm_t)), make_v4(0.0f, 0.5f, 0.0f, 1.0f));

            v3_t square_v0 = add(plane->lm_origin, mul(scale_x, plane->lm_s));
            v3_t square_v1 = add(plane->lm_origin, mul(scale_y, plane->lm_t));
            v3_t square_v2 = v3_add3(plane->lm_origin, mul(scale_x, plane->lm_s), mul(scale_y, plane->lm_t));

            r_immediate_line(square_v0, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));
            r_immediate_line(square_v1, square_v2, make_v4(0.5f, 0.0f, 0.5f, 1.0f));

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

                r_immediate_line(pixel_v0, pixel_v1, COLORF_RED);
                r_immediate_line(pixel_v0, pixel_v2, COLORF_RED);
                r_immediate_line(pixel_v2, pixel_v3, COLORF_RED);
                r_immediate_line(pixel_v1, pixel_v3, COLORF_RED);
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
											r_immediate_line(vertex->o, point_light->p, 
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
										r_immediate_arrow_gradient(next_vertex->o, vertex->o, end_color, start_color);
									}
									else
									{
										r_immediate_line_gradient(vertex->o, next_vertex->o, start_color, end_color);
									}
								}
							}

							vertex_index += 1;
						}
					}
				}
			}
		}

        r_immediate_flush();
    }
}

DREAM_INLINE void fullscreen_show_timings(void)
{
    UI_SCALAR(UI_SCALAR_TEXT_ALIGN_X, 0.5f)
    {
        render_timings_t timings;
        render->get_timings(&timings);

        float total = 0.0f;
        for (int i = 1; i < RENDER_TS_COUNT; i++)
        {
            ui_label(Sf("%s: %.02fms", render_timestamp_names[i], 1000.0*timings.dt[i]));
            total += timings.dt[i];
        }

        ui_label(Sf("total: %.02fms", 1000.0*total));
    }
}

DREAM_INLINE void update_and_render_ui_demo(void)
{
	ui_demo_panel_t *demo = &editor.ui_demo;

	if (!demo->window.open)
		return;

	UI_WINDOW(&demo->window)
	{
		ui_slider    (S("Float Slider"), &demo->slider_f32, -1.0f, 1.0f);
		ui_slider_int(S("Int Slider"), &demo->slider_i32, 0, 8);

		ui_slider(S("UI Widget Margin"), &ui.base_scalars[UI_SCALAR_WIDGET_MARGIN], 0.0f, 8.0f);
		ui_slider(S("UI Text Margin"), &ui.base_scalars[UI_SCALAR_TEXT_MARGIN], 0.0f, 8.0f);
		ui_slider(S("UI Roundedness"), &ui.base_scalars[UI_SCALAR_ROUNDEDNESS], 0.0f, 16.0f);
		ui_slider(S("UI Animation Stiffness"), &ui.base_scalars[UI_SCALAR_ANIMATION_STIFFNESS], 1.0f, 1024.0f);
		ui_slider(S("UI Animation Dampen"), &ui.base_scalars[UI_SCALAR_ANIMATION_DAMPEN], 1.0f, 128.0f);
	}
}

DREAM_INLINE void fullscreen_update_and_render_top_editor_bar(void)
{
    rect2_t bar = ui_cut_top(editor.fullscreen_layout, 32.0f);

    rect2_t collision_bar = bar;
    collision_bar.min.y -= 32.0f;

    bool mouse_hover = rect2_contains_point(collision_bar, ui.mouse_p);
    editor.bar_openness = ui_animate_towards(editor.bar_openness, mouse_hover ? 1.0f : 0.0f);

    if (editor.bar_openness > 0.0001f)
    {
        float height = rect2_height(bar);

        bar = rect2_add_offset(bar, make_v2(0.0f, (1.0f - editor.bar_openness)*height));

        r_immediate_rect2_filled(bar, ui_color(UI_COLOR_WINDOW_BACKGROUND));
        r_immediate_flush();

        rect2_t inner_bar = ui_shrink(&bar, ui_scalar(UI_SCALAR_WIDGET_MARGIN));
        UI_PANEL(inner_bar)
        {
            typedef struct menu_t
            {
                string_t name;
                bool *toggle;
				editor_kind_t editor;
            } menu_t;

            menu_t menus[] = {
                {
                    .name = S("Timings (F1)"),
                    .toggle = &editor.show_timings,
					.editor = EDITOR_TIMINGS,
                },
                {
                    .name = S("Lightmap Editor (F2)"),
                    .toggle = &editor.lm_editor.window.open,
					.editor = EDITOR_LIGHTMAP,
                },
                {
                    .name = S("Convex Hull Tester (F3)"),
                    .toggle = &editor.hull_test.window.open,
					.editor = EDITOR_CONVEX_HULL,
                },
                {
                    .name = S("UI Demo (F4)"),
                    .toggle = &editor.ui_demo.window.open,
					.editor = EDITOR_UI_DEMO,
                },
            };

            ui_set_next_rect(ui_cut_left(&bar, ui_label_width(S("Menus:  "))));
            ui_label(S("Menus:"));

            for (size_t i = 0; i < ARRAY_COUNT(menus); i++)
            {
                menu_t *menu = &menus[i];
                
                float width = ui_label_width(menu->name);

                bool active = *menu->toggle;

                if (active)
                {
                    ui_push_color(UI_COLOR_BUTTON_IDLE, ui_color(UI_COLOR_BUTTON_ACTIVE));
                }

                ui_set_next_rect(ui_cut_left(&bar, width));
                if (ui_button(menu->name))
                {
                    *menu->toggle = !*menu->toggle;
					if (*menu->toggle)
					{
						bring_editor_to_front(menu->editor);
					}
                }

                if (active)
                {
                    ui_pop_color(UI_COLOR_BUTTON_IDLE);
                }
            }
        }
    }
}

void update_and_render_in_game_editor(void)
{
	if (!editor.initialized)
	{
		editor.initialized = true;

		send_editor_to_back(EDITOR_LIGHTMAP);
		editor.lm_editor.window.rect = rect2_from_min_dim(make_v2(32, 32), make_v2(512, 512));

		send_editor_to_back(EDITOR_CONVEX_HULL);
		editor.hull_test.window.rect = rect2_from_min_dim(make_v2(64, 64), make_v2(512, 512));

		send_editor_to_back(EDITOR_UI_DEMO);
		editor.ui_demo.window.rect = rect2_from_min_dim(make_v2(96, 96), make_v2(512, 512));
	}

    if (ui_button_pressed(BUTTON_F1))
        editor.show_timings = !editor.show_timings;

    if (ui_button_pressed(BUTTON_F2))
		editor.lm_editor.window.open = !editor.lm_editor.window.open;

    if (ui_button_pressed(BUTTON_F3))
		editor.hull_test.window.open = !editor.hull_test.window.open;

    if (ui_button_pressed(BUTTON_F4))
		editor.ui_demo.window.open = !editor.ui_demo.window.open;

	copy_struct(&editor.window_order, &editor.next_window_order);

    int res_x, res_y;
    render->get_resolution(&res_x, &res_y);

    rect2_t fullscreen_rect = {
        .min = { 0, 0 },
        .max = { (float)res_x, (float)res_y },
    };

	for (size_t i = 0; i < stack_count(editor.window_order); i++)
	{
		editor_kind_t kind = editor.window_order.values[i];

		switch (kind)
		{
			case EDITOR_LIGHTMAP:    { update_and_render_lightmap_editor(); } break;
			case EDITOR_CONVEX_HULL: { update_and_render_convex_hull_test_panel(); } break;
			case EDITOR_UI_DEMO:     { update_and_render_ui_demo(); } break;
		}
	}

    UI_SCALAR(UI_SCALAR_WIDGET_MARGIN, 0.0f)
    ui_panel_begin(fullscreen_rect);
	{
		editor.fullscreen_layout = ui_layout_rect();

		fullscreen_update_and_render_top_editor_bar();

		if (editor.show_timings)
			fullscreen_show_timings();
	}
	ui_panel_end();
}
