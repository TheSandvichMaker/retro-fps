// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

void editor_init_convex_hull_debugger(editor_convex_hull_debugger_t *debugger)
{
	debugger->automatically_recalculate_hull = true;
	debugger->random_seed              = 1;
	debugger->point_count              = 12;
	debugger->show_points              = true;
	debugger->show_processed_edge      = true;
	debugger->show_new_triangle        = true;
	debugger->show_triangles           = true;
	debugger->show_duplicate_triangles = true;
	debugger->show_wireframe           = true;
	debugger->test_tetrahedron_index   = -1;
	debugger->current_step_index       = 9999;

	debugger->brute_force_min_point_count            = 4;
	debugger->brute_force_max_point_count            = 256;
	debugger->brute_force_iterations_per_point_count = 4096;

	debugger->initialized = true;
}

fn_local void generate_new_random_convex_hull(editor_convex_hull_debugger_t *debugger, size_t point_count, random_series_t *r)
{
	triangle_mesh_t *mesh  = &debugger->mesh;
	hull_debug_t    *debug = &debugger->debug;

	m_scoped_temp
	{
		v3_t *random_points = m_alloc_array_nozero(temp, point_count, v3_t);

		for (size_t i = 0; i < point_count; i++)
		{
			random_points[i] = add(make_v3(512.0f, 512.0f, 512.0f), mul(128.0f, random_in_unit_cube(r)));
		}

		bool at_final_step = false;
		if (debug->step_count > 0 &&
			debugger->current_step_index >= (int)debug->step_count)
		{
			at_final_step = true;
		}

		m_reset(&debugger->mesh_arena);

		*mesh = calculate_convex_hull_debug(&debugger->mesh_arena, point_count, random_points, debug);
		convex_hull_do_extended_diagnostics(mesh, debug);

		if (at_final_step)
		{
			debugger->current_step_index = (int)debug->step_count;
		}
	}
}

/*
fn_local void hull_render_debug_triangles(convex_hull_debugger_t *debugger, r_context_t *rc, hull_debug_step_t *step, bool final_step, bool render_degenerate, bool render_wireframe)
{
	hull_debug_t             *debug     = &debugger->debug;
	hull_diagnostic_result_t *diag      = debug->diagnostics;

	m_scoped_temp
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

			if (debugger->show_triangles)                                 show = true;
			if (debugger->show_new_triangle && triangle->added_this_step) show = true;
			if (!show) continue;

			if (diag->triangle_is_degenerate[triangle_index] != render_degenerate) 
				continue;

			v3_t a = triangle->t.a;
			v3_t b = triangle->t.b;
			v3_t c = triangle->t.c;

			v4_t color = base_color;

			if (!final_step && debugger->show_new_triangle && triangle->added_this_step)
				color = new_triangle_color;

			if (debugger->show_duplicate_triangles &&
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
				r_immediate_line(rc, a, b, color);
				r_immediate_line(rc, a, c, color);
				r_immediate_line(rc, b, c, color);
			}
			else
			{
				triangle_t t = { a, b, c };
				r_immediate_triangle(rc, t, color);
			}
		}
	}
}

void load_convex_hull_debug(triangle_mesh_t *mesh, hull_debug_t *debug)
{
	(void)mesh;
	(void)debug;
#if 0
    // FIXME:
    // FIXME:
    // FIXME:
    // FIXME:
	hull_test_panel_t *hull_test = &editor.hull_test;
    hull_test->mesh = *mesh;
    hull_test->debug = *debug;
#endif
}

void convex_hull_debugger_update_and_render(convex_hull_debugger_t *debugger)
{
	hull_debug_t *debug = &debugger->debug;

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
		if (debugger->show_points)
		{
#if 0
			R_VIEW     (rc, game_view) 
            R_IMMEDIATE(rc, imm)
            {
                imm->topology  = R_TOPOLOGY_LINELIST;
                imm->use_depth = true;

                for (size_t i = 0; i < debug->initial_points_count; i++)
                {
                    v3_t p = debug->initial_points[i];

                    v4_t color = ((int)i == debugger->test_tetrahedron_index ? COLORF_ORANGE : COLORF_RED);

                    if (point_fully_contained[i])
                    {
                        color.xyz = mul(color.xyz, 0.75f);
                    }

                    r_immediate_rect3_outline(rc, rect3_center_radius(p, make_v3(1, 1, 1)), color);
                }
            }
#endif
		}

		//

#if 0
		R_VIEW     (rc, game_view) 
        R_IMMEDIATE(rc, imm)
        {
            imm->topology  = R_TOPOLOGY_TRIANGLELIST;
            imm->shader    = R_SHADER_DEBUG_LIGHTING;
            imm->use_depth = true;
            imm->cull_mode = R_CULL_BACK;

            int step_index = 0;

            for (hull_debug_step_t *step = debugger->debug.first_step;
                 step;
                 step = step->next)
            {
                if (step_index == MIN(debugger->current_step_index, (int)debug->step_count - 1))
                {
                    bool final_step = (debugger->current_step_index == (int)debug->step_count);

                    hull_render_debug_triangles(debugger, rc, step, final_step, false, false);
                    hull_render_debug_triangles(debugger, rc, step, final_step, true, false);
                }

                step_index++;
            }
        }

		//

		R_VIEW     (rc, game_view) 
        R_IMMEDIATE(rc, imm)
        {
            imm->topology  = R_TOPOLOGY_LINELIST;
            imm->shader    = R_SHADER_FLAT;
		    imm->use_depth = true;

            int step_index = 0;

            for (hull_debug_step_t *step = debugger->debug.first_step;
                 step;
                 step = step->next)
            {
                if (step_index == MIN(debugger->current_step_index, (int)debug->step_count - 1))
                {
                    bool final_step = (debugger->current_step_index == (int)debug->step_count);

                    if (debugger->show_wireframe)
                    {
                        hull_render_debug_triangles(debugger, rc, step, final_step, false, true);
                        hull_render_debug_triangles(debugger, rc, step, final_step, true, true);
                    }

                    if (!final_step)
                    {
                        for (hull_debug_edge_t *edge = step->first_edge;
                             edge;
                             edge = edge->next)
                        {
                            bool show = false;

                            if (debugger->show_processed_edge      &&  edge->processed_this_step) show = true;
                            if (debugger->show_non_processed_edges && !edge->processed_this_step) show = true;
                            if (!show) continue;

                            v3_t a = edge->e.a;
                            v3_t b = edge->e.b;

                            r_immediate_line(rc, a, b, (edge->processed_this_step ? COLORF_WHITE : COLORF_BLUE));
                        }
                    }
                }

                step_index++;
            }
        }
#endif
	}
}

*/

void editor_do_convex_hull_debugger_window(editor_convex_hull_debugger_t *debugger, editor_window_t *window)
{
	(void)window;

	ui_scrollable_region_flags_t scroll_flags = 
		UiScrollableRegionFlags_scroll_vertical|
		UiScrollableRegionFlags_draw_scroll_bar;

	rect2_t rect = window->rect;
	rect = rect2_shrink(rect, 1.0f);

	rect2_t content_rect = ui_scrollable_region_begin_ex(&window->scroll_region, rect, scroll_flags);

	content_rect = rect2_shrink(content_rect, 1.0f);

	ui_row_builder_t builder = ui_make_row_builder(content_rect);

	triangle_mesh_t *mesh  = &debugger->mesh;
	hull_debug_t    *debug = &debugger->debug;

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

	ui_row_header(&builder, S("Generate Random Hull"));

	bool changed = false;
	changed |= ui_row_slider_int(&builder, S("Random Seed"), &debugger->random_seed, 1, 128);
	changed |= ui_row_slider_int(&builder, S("Point Count"), &debugger->point_count, 8, 256);

	ui_row_checkbox(&builder, S("Automatically Recalculate Hull"), &debugger->automatically_recalculate_hull);

	bool should_recalculate = ui_row_button(&builder, S("Calculate Random Convex Hull"));
	if (debugger->automatically_recalculate_hull && changed)                 should_recalculate = true;
	if (debugger->automatically_recalculate_hull && !debugger->initialized) should_recalculate = true;

	if (should_recalculate)
	{
		random_series_t r = { (uint32_t)debugger->random_seed };
		generate_new_random_convex_hull(debugger, (size_t)debugger->point_count, &r);
	}

	if (mesh->triangle_count > 0)
	{
		if (ui_row_button(&builder, S("Delete Convex Hull Data")))
		{
			m_release(&debugger->mesh_arena);
			m_release(&debugger->debug.arena);
			zero_struct(mesh);
			zero_struct(debug);
		}
	}

	// ui_seperator();
	ui_row_header(&builder, S("Brute Force Tester"));
	ui_row_slider_int(&builder, S("Min Points"), &debugger->brute_force_min_point_count, 4, 256);
	ui_row_slider_int(&builder, S("Max Points"), &debugger->brute_force_max_point_count, 4, 256);
	ui_row_slider_int(&builder, S("Iterations Per Count"), &debugger->brute_force_iterations_per_point_count, 1, 4096);

	if (ui_row_button(&builder, S("Run Brute Force Test")))
	{
		random_series_t r = { (uint32_t)debugger->random_seed };

		debugger->brute_force_triggered_success_timer = 0.0f;
		debugger->brute_force_triggered_error_timer   = 0.0f;

		for (int point_count = debugger->brute_force_min_point_count;
			 point_count <= debugger->brute_force_max_point_count;
			 point_count++)
		{
			generate_new_random_convex_hull(debugger, (size_t)point_count, &r);

			if (ALWAYS(debug->diagnostics))
			{
				hull_diagnostic_result_t *diagnostics = debug->diagnostics;
				if (diagnostics->degenerate_hull)
				{
					debugger->brute_force_triggered_error_timer = 10.0f;
					break;
				}
			}
		}

		if (debugger->brute_force_triggered_error_timer <= 0.0f)
		{
			debugger->brute_force_triggered_success_timer = 5.0f;
		}
	}

	if (debugger->brute_force_triggered_success_timer > 0.0f)
	{
		UI_Color(UiColor_text, make_v4(0.5f, 1.0f, 0.2f, 1.0f))
		ui_row_label(&builder, S("Brute force test passed."));

		debugger->brute_force_triggered_success_timer -= ui->dt;
	}

	if (debugger->brute_force_triggered_error_timer > 0.0f)
	{
		UI_Color(UiColor_text, COLORF_RED)
		ui_row_label(&builder, S("BRUTE FORCE TEST FOUND A DEGENERATE HULL!?!?!"));

		debugger->brute_force_triggered_error_timer -= ui->dt;
	}

	if (degenerate_hull)
	{
		UI_Color(UiColor_text, COLORF_RED)
		ui_row_label(&builder, S("!! DEGENERATE CONVEX HULL !!"));
		if (triangle_is_degenerate[0])
		{
			UI_Color(UiColor_text, COLORF_RED)
			ui_row_label(&builder, S("THE FIRST TRIANGLE IS DEGENERATE, THAT'S NO GOOD"));
		}
		ui_row_label(&builder, Sf("Degenerate Triangle Count: %d", degenerate_triangle_count));
	}

	if (duplicate_triangle_count > 0)
	{
		UI_Color(UiColor_text, COLORF_ORANGE)
		ui_row_label(&builder, Sf("There are %d duplicate triangles!", duplicate_triangle_count));
	}

	if (no_area_triangle_count > 0)
	{
		UI_Color(UiColor_text, COLORF_ORANGE)
		ui_row_label(&builder, Sf("There are %d triangles with (nearly) 0 area!", no_area_triangle_count));
		if (triangle_is_degenerate[0])
		{
			UI_Color(UiColor_text, COLORF_ORANGE)
			ui_row_label(&builder, S("The first triangle has no area, which seems bad"));
		}
	}

	if (debug->step_count > 0)
	{
		// ui_seperator();
		ui_row_header(&builder, S("Step-By-Step Visualizer"));
		ui_row_checkbox(&builder, S("Show initial points"), &debugger->show_points);
		ui_row_checkbox(&builder, S("Show processed edge"), &debugger->show_processed_edge);
		ui_row_checkbox(&builder, S("Show non-processed edges"), &debugger->show_non_processed_edges);
		ui_row_checkbox(&builder, S("Show new triangle"), &debugger->show_new_triangle);
		ui_row_checkbox(&builder, S("Show all triangles"), &debugger->show_triangles);
		ui_row_checkbox(&builder, S("Show duplicate triangles"), &debugger->show_duplicate_triangles);
		ui_row_checkbox(&builder, S("Show wireframe"), &debugger->show_wireframe);
		ui_row_slider_int(&builder, S("Step"), &debugger->current_step_index, 0, (int)(debugger->debug.step_count));

		if (debugger->current_step_index > (int)debugger->debug.step_count)
			debugger->current_step_index = (int)debugger->debug.step_count;

		int step_index = 0;
		hull_debug_step_t *step = debug->first_step;
		for (;
			 step;
			 step = step->next)
		{
			if (step_index == MIN(debugger->current_step_index, (int)debug->step_count - 1))
			{
				break;
			}
			step_index++;
		}

		if (ALWAYS(step))
		{
			ui_row_label(&builder, Sf("edge count: %zu", step->edge_count));
			ui_row_label(&builder, Sf("triangle count: %zu", step->triangle_count));

			ui_row_slider_int(&builder, S("Test Tetrahedron Index"), &debugger->test_tetrahedron_index, -1, (int)(debug->initial_points_count) - 1);

			if (debugger->test_tetrahedron_index > (int)debug->initial_points_count - 1)
				debugger->test_tetrahedron_index = (int)debug->initial_points_count - 1;

			if (debugger->test_tetrahedron_index >= 0)
			{
				triangle_t t = step->first_triangle->t;
				ASSERT(step->first_triangle->added_this_step);

				v3_t p = debug->initial_points[debugger->test_tetrahedron_index];

				float volume = tetrahedron_signed_volume(t.a, t.b, t.c, p);
				ui_row_label(&builder, Sf("Tetrahedron Volume: %f", volume));

				float area = triangle_area_sq(t.a, t.b, p);
				ui_row_label(&builder, Sf("Squared Triangle Area: %f", area));
			}
		}
	}

	ui_scrollable_region_end(&window->scroll_region, builder.rect);
}

