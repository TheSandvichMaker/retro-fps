void editor_texture_viewer_ui(editor_texture_viewer_t *viewer, rect2_t rect)
{
	rect2_t content_rect = rect2_cut_margins(rect, ui_sz_pix(ui_scalar(UiScalar_outer_window_margin)));

	ui_row_builder_t builder = ui_make_row_builder(
		ui_scrollable_region_begin_ex(
			&viewer->scroll_region, 
			content_rect,
			UiScrollableRegionFlags_scroll_vertical|UiScrollableRegionFlags_draw_scroll_bar|UiScrollableRegionFlags_always_draw_vertical_scroll_bar));

	if (ui_row_collapsable_region(&builder, S("Buffers"), &viewer->buffer_view_open))
	{
		for (pool_iter_t buffer_iter = rhi_buffer_iter(); pool_iter_valid(&buffer_iter); pool_iter_next(&buffer_iter))
		{
			rhi_buffer_t buffer_handle = CAST_HANDLE(rhi_buffer_t, pool_get_handle(buffer_iter.pool, buffer_iter.data));

			const rhi_buffer_desc_t *desc = rhi_get_buffer_desc(buffer_handle);
			ui_row_label(&builder, desc->debug_name);
		}
	}

	if (ui_row_collapsable_region(&builder, S("Textures"), &viewer->texture_view_open))
	{
		float content_width        = rect2_width(builder.rect);
		float texture_width        = 64.0f;
		float texture_width_padded = texture_width + 4.0f;
		int   textures_per_row     = (int)floorf(content_width / texture_width_padded);

		float texture_size = ui_size_to_width(builder.rect, ui_sz_pct(1.0f / (float)textures_per_row));

		pool_iter_t texture_iter = rhi_texture_iter();
	
		while (pool_iter_valid(&texture_iter))
		{
			ui_push_scalar(UiScalar_row_margin, 0.0f);
			rect2_t row = ui_row_ex(&builder, texture_size, false);
			ui_pop_scalar (UiScalar_row_margin);
	
			for (int i = 0; i < textures_per_row; i++)
			{
				if (pool_iter_valid(&texture_iter))
				{
					rhi_texture_t     texture_handle = CAST_HANDLE(rhi_texture_t, pool_get_handle(texture_iter.pool, texture_iter.data));
					rhi_texture_srv_t texture_srv    = rhi_get_texture_srv(texture_handle);

					const rhi_texture_desc_t *desc = rhi_get_texture_desc(texture_handle);

					rect2_t texture_rect;
					rect2_cut_from_left(row, ui_sz_aspect(1.0f), &texture_rect, &row);

					rect2_t texture_rect_no_margins = texture_rect;

					texture_rect = rect2_cut_margins(texture_rect, ui_sz_pix(2.0f));

					ui_draw_image   (texture_rect, ui->checkerboard_texture_srv);
					ui_draw_image_ex(texture_rect, texture_srv, pixel_format_is_hdr(desc->format) ? UiDrawImage_tonemap : 0);

					ui_id_t id = ui_id_u64(texture_handle.value);

					if (ui_mouse_in_rect(texture_rect_no_margins))
					{
						ui_set_next_hot(id);
					}

					if (ui_is_hot(id))
					{
						ui_draw_rect_outline(rect2_add_radius(texture_rect, v2s(1.0f)), ui_color(UiColor_button_hot), 1.0f);

						if (ui_button_pressed(UiButton_left, true))
						{
							if (viewer->current_texture.value == texture_handle.value)
							{
								viewer->current_texture.value = 0;
							}
							else
							{
								viewer->current_texture = texture_handle;
							}
						}

						ui_push_sub_layer();
						ui_push_clip_rect_ex(ui->ui_area, UiClipRectFlag_absolute);

						v2_t popup_p = add(ui->input.mouse_p, make_v2(16, 16));

						float text_width  = ui_text_width(ui_font(UiFont_default), desc->debug_name);
						float popup_width = max(text_width + 4.0f, 256.0f);
				
						ui_push_sub_layer();

						ui_row_builder_t popup_builder = ui_make_row_builder_ex(rect2_from_min_dim(popup_p, make_v2(popup_width, 0.0f)), &(ui_row_builder_params_t){
							.flags = UiRowBuilder_insert_row_separators,
						});
						ui_row_label(&popup_builder, desc->debug_name);
						ui_row_labels2(&popup_builder, S("Dimensions:"), Sf("%u", desc->dimension));
						ui_row_labels2(&popup_builder, S("Width:"), Sf("%u", desc->width));
						ui_row_labels2(&popup_builder, S("Height:"), Sf("%u", desc->height));
						ui_row_labels2(&popup_builder, S("Depth:"), Sf("%u", desc->depth));
						ui_row_labels2(&popup_builder, S("Mip Levels:"), Sf("%u", desc->mip_levels));
						ui_row_labels2(&popup_builder, S("Format:"), Sf("%cs", pixel_format_to_string(desc->format)));
						ui_row_labels2(&popup_builder, S("Sample Count:"), Sf("%u", desc->sample_count));

						ui_pop_sub_layer();

						v4_t bg_color = ui_color(UiColor_window_background);
						bg_color.w = 0.2f;

						rect2_t popup_rect = rect2_add_radius(rect2_uninvert(popup_builder.rect), v2s(2.0f));
						ui_draw_rect_shadow(popup_rect, bg_color, 1.0f, 8.0f);

						ui_pop_clip_rect();
						ui_pop_sub_layer();
					}

					if (viewer->current_texture.value == texture_handle.value)
					{
						ui_draw_rect_outline(rect2_add_radius(texture_rect, v2s(1.0f)), ui_color(UiColor_button_active), 1.0f);
					}

					pool_iter_next(&texture_iter);
				}
				else
				{
					break;
				}
			}
		}
	}

	ui_scrollable_region_end(&viewer->scroll_region, builder.rect);
}

void editor_texture_viewer_render(editor_texture_viewer_t *viewer, r1_view_t *view)
{
	(void)view;

	if (RESOURCE_HANDLE_VALID(viewer->current_texture))
	{
		const rhi_texture_desc_t *desc = rhi_get_texture_desc(viewer->current_texture);

		if (desc)
		{
			rhi_texture_srv_t texture_srv = rhi_get_texture_srv(viewer->current_texture);

			ui_push_sub_layer();
			ui_push_clip_rect_ex(ui->ui_area, UiClipRectFlag_absolute);

			rect2_t area = rect2_cut_margins(ui->ui_area, ui_sz_pix(16.0f));

			float min_width = 128.0f;
			float max_width = (1.0f / 3.0f) * rect2_width(ui->ui_area);

			float width  = flt_clamp((float)desc->width, min_width, max_width);
			float height = ((float)desc->height / (float)desc->width)*width;

			rect2_t texture_view_rect = make_rect2_max_dim(area.max, make_v2(width, height));

			UI_Scalar(UiScalar_roundedness, 0.0f)
			{
				ui_draw_rect(rect2_add_radius(texture_view_rect, v2s(2.0f)), COLORF_RED);
				ui_draw_rect(texture_view_rect, COLORF_BLACK);
			}

			ui_draw_image_ex(texture_view_rect, texture_srv, pixel_format_is_hdr(desc->format) ? UiDrawImage_tonemap : 0);

			ui_pop_clip_rect();
			ui_pop_sub_layer();
		}
	}
}