void editor_init_lightmap_stuff(editor_lightmap_state_t *state)
{
	state->max_display_recursion = 10;
}

void editor_do_lightmap_window(editor_lightmap_state_t *lm_editor, editor_window_t *window, rect2_t rect)
{
	(void)window;

	ui_scrollable_region_flags_t scroll_flags = 
		UiScrollableRegionFlags_scroll_vertical|
		UiScrollableRegionFlags_draw_scroll_bar;

	rect = rect2_shrink(rect, 1.0f);

	rect2_t content_rect = ui_scrollable_region_begin_ex(&window->scroll_region, rect, scroll_flags);

	content_rect = rect2_shrink(content_rect, 1.0f);

	ui_row_builder_t builder = ui_make_row_builder(content_rect);

    gamestate_t  *game       = g_game;
    map_t        *map        = game->map;
    map_entity_t *worldspawn = game->worldspawn;

    v3_t  sun_color      = v3_normalize(v3_from_key(map, worldspawn, S("sun_color")));
    float sun_brightness = float_from_key(map, worldspawn, S("sun_brightness"));

    sun_color = mul(sun_brightness, sun_color);

    v3_t ambient_color = v3_from_key(map, worldspawn, S("ambient_color"));
    ambient_color = mul(1.0f / 255.0f, ambient_color);

	lum_state_flags_t flags = map->lightmap_state ? atomic_load(&map->lightmap_state->flags) : 0;

	if (!map->lightmap_state || !(flags & LumStateFlag_finalized))
	{
		ui_row_header(&builder, S("Bake Settings"));

		local_persist int bake_preset              = 0;
		local_persist int ray_count                = 2;
		local_persist int ray_recursion            = 2;
		local_persist int fog_light_sample_count   = 2;
		local_persist int fogmap_scale_index       = 1;
		local_persist bool use_dynamic_sun_shadows = true;

		local_persist string_t preset_labels[] = { Sc("Crappy"), Sc("Acceptable"), Sc("Excessive") };
		if (ui_row_radio_buttons(&builder, S("Preset"), &bake_preset, preset_labels, ARRAY_COUNT(preset_labels)))
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

		ui_row_slider_int(&builder, S("Rays Per Pixel"), &ray_count, 1, 32);
		ui_row_slider_int(&builder, S("Max Recursion"), &ray_recursion, 1, 8);
		ui_row_slider_int(&builder, S("Fog Light Sample Count"), &fog_light_sample_count, 1, 8);

		local_persist int fogmap_scales[] = {
			32, 16, 8, 4,
		};

		local_persist string_t fogmap_scale_labels[] = {
			Sc("32"), Sc("16"), Sc("8"), Sc("4"),
		};

		ui_row_radio_buttons(&builder, S("Fogmap Scale"), &fogmap_scale_index, fogmap_scale_labels, ARRAY_COUNT(fogmap_scales));

		int actual_fogmap_scale = fogmap_scales[fogmap_scale_index];

		ui_row_checkbox(&builder, S("Dynamic Sun Shadows"), &use_dynamic_sun_shadows);

		ui_id_t bake_cancel_button = ui_id(S("Bake Lighting / Cancel"));

		if (!map->lightmap_state)
		{
			ui_set_next_id(bake_cancel_button);
			if (ui_row_button(&builder, S("Bake Lighting")))
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
			if (ui_row_button(&builder, S("Cancel")))
			{
				bake_cancel(map->lightmap_state);

				for (size_t poly_index = 0; poly_index < map->poly_count; poly_index++)
				{
					map_poly_t *poly = &map->polys[poly_index];
					if (RESOURCE_HANDLE_VALID(poly->lightmap))
					{
#if 0
						render->destroy_texture(poly->lightmap);
#endif
                        NULLIFY_HANDLE(&poly->lightmap);
					}
				}

#if 0
				render->destroy_texture(map->fogmap);
#endif
                NULLIFY_HANDLE(&map->fogmap);

				map->lightmap_state = NULL;
			}
		}
	}

	if (map->lightmap_state && (flags & LumStateFlag_finalized))
	{
		lum_bake_state_t *state = map->lightmap_state;

		double time_elapsed = state->final_bake_time;
		int minutes = (int)floor(time_elapsed / 60.0);
		int seconds = (int)floor(time_elapsed - 60.0*minutes);
		int microseconds = (int)floor(1000.0*(time_elapsed - 60.0*minutes - seconds));

		ui_row_header(&builder, S("Bake Results"));

		UI_Scalar(UiScalar_text_align_x, 0.0f)
		{
			ui_row_label(&builder, Sf("Total Bake Time:  %02u:%02u:%03u", minutes, seconds, microseconds));
			ui_row_label(&builder, Sf("Fogmap Resolution: %u %u %u", map->fogmap_w, map->fogmap_h, map->fogmap_d));
		}

		if (ui_row_button(&builder, S("Clear Lightmaps")))
		{
			for (size_t poly_index = 0; poly_index < map->poly_count; poly_index++)
			{
				map_poly_t *poly = &map->polys[poly_index];
				if (RESOURCE_HANDLE_VALID(poly->lightmap))
				{
#if 0
					render->destroy_texture(poly->lightmap);
#endif
					NULLIFY_HANDLE(&poly->lightmap);
				}
			}

#if 0
			render->destroy_texture(map->fogmap);
#endif
			NULLIFY_HANDLE(&map->fogmap);

			release_bake_state(map->lightmap_state);
			map->lightmap_state = NULL;
		}
	}

	if (map->lightmap_state)
	{
		if (flags & LumStateFlag_finalized)
		{
			ui_row_header(&builder, S("Lightmap Debugger"));

			ui_row_checkbox(&builder, S("Enabled"), &lm_editor->debug_lightmaps);
			ui_row_checkbox(&builder, S("Show Direct Light Rays"), &lm_editor->show_direct_light_rays);
			ui_row_checkbox(&builder, S("Show Indirect Light Rays"), &lm_editor->show_indirect_light_rays);
			ui_row_checkbox(&builder, S("Fullbright Rays"), &lm_editor->fullbright_rays);
			ui_row_checkbox(&builder, S("No Ray Depth Test"), &lm_editor->no_ray_depth_test);

			ui_row_slider_int(&builder, S("Min Recursion Level"), &lm_editor->min_display_recursion, 0, 16);
			ui_row_slider_int(&builder, S("Max Recursion Level"), &lm_editor->max_display_recursion, 0, 16);

			if (lm_editor->selected_poly)
			{
#if 0
				map_poly_t *poly = lm_editor->selected_poly;

				r_texture_desc_t desc;
				render->describe_texture(poly->lightmap, &desc);

				ui_push_scalar(UiScalar_text_align_x, 0.0f);

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

				ui_pop_scalar(UiScalar_text_align_x);

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
#endif
			}
		}
		else
		{
			float progress = bake_progress(map->lightmap_state);

			ui_row_progress_bar(&builder, Sf("bake progress: %u / %u (%.02f%%)", map->lightmap_state->jobs_completed, map->lightmap_state->job_count, 100.0f*progress), progress);

			hires_time_t current_time = os_hires_time();
			double time_elapsed = os_seconds_elapsed(map->lightmap_state->start_time, current_time);
			int minutes = (int)floor(time_elapsed / 60.0);
			int seconds = (int)floor(time_elapsed - 60.0*minutes);

			ui_row_label(&builder, Sf("time elapsed:  %02u:%02u", minutes, seconds));
		}
	}

	ui_scrollable_region_end(&window->scroll_region, builder.rect);
}

/*
fn_local void render_lm_editor(void)
{
    gamestate_t *game = g_game;
    // player_t *player = game->player;
    map_t    *map    = game->map;

    map_entity_t *worldspawn = game->worldspawn;

    v3_t  sun_color      = v3_normalize(v3_from_key(map, worldspawn, S("sun_color")));
    float sun_brightness = float_from_key(map, worldspawn, S("sun_brightness"));

    sun_color = mul(sun_brightness, sun_color);

    v3_t ambient_color = v3_from_key(map, worldspawn, S("ambient_color"));
    ambient_color = mul(1.0f / 255.0f, ambient_color);

    editor_lightmap_state_t *lm_editor = &editor.lm_editor;

    if (lm_editor->debug_lightmaps)
    {
#if 0
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
#if 0
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
#endif
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
#endif
    }
}
*/
