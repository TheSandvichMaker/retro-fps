// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

ui_layout_t *ui_get_layout(void)
{
	ASSERT_MSG(ui->layout, "You have to equip a layout before calling any layout functions");
	return ui->layout;
}

ui_layout_t ui_make_layout(rect2_t starting_rect)
{
	ui_layout_t layout = {
		.rect = starting_rect,
	};
	return layout;
}

void ui_equip_layout(ui_layout_t *layout)
{
	DEBUG_ASSERT_MSG(!ui->layout, "Please unequip your old layout first");
	ui->layout = layout;
}

void ui_unequip_layout(void)
{
	DEBUG_ASSERT_MSG(ui->layout, "You don't have a layout equipped!");
	ui->layout = NULL;
}

rect2_t layout_rect(void)
{
	ui_layout_t *layout = ui_get_layout();
	return layout->rect;
}

void layout_prepare_even_spacing(uint32_t item_count)
{
	ui_layout_t *layout = ui_get_layout();

	DEBUG_ASSERT_MSG(layout->prepared_rect_count == 0, 
					 "Calling layout_prepare_even_spacing without having consumed "
					 "previous prepared rects seems like a mistake");

	DEBUG_ASSERT_MSG(layout->flow != Flow_none, "You need to set a flow before doing UI cuts");

	ui_size_t size = ui_sz_pct(1.0f / (float)item_count);

	for (size_t i = 0; i < item_count; i++)
	{
		if (!ui->first_free_prepared_rect)
		{
			ui->first_free_prepared_rect = m_alloc_struct_nozero(&ui->arena, ui_prepared_rect_t);
			ui->first_free_prepared_rect->next = NULL;
		}

		ui_prepared_rect_t *prep = sll_pop(ui->first_free_prepared_rect);

		rect2_t active, remainder;
		rect2_cut(layout->rect, layout->flow, size, &active, &remainder);

		prep  ->rect = active;
		layout->rect = remainder;

		layout->prepared_rect_count += 1;
		sll_push_back(layout->first_prepared_rect, layout->last_prepared_rect, prep);
	}
}

rect2_t layout_place_widget(v2_t widget_size)
{
	ui_layout_t *layout = ui_get_layout();

	rect2_t rect = {0};

	if (layout->prepared_rect_count > 0)
	{
		DEBUG_ASSERT_MSG(!layout->flow == Flow_justify, "You can't justify and have prepared rects at the same time.");

		ui_prepared_rect_t *prep = sll_pop(layout->first_prepared_rect);
		layout->prepared_rect_count -= 1;

		if (!ui->first_free_prepared_rect)
		{
			ASSERT(layout->prepared_rect_count == 0);
			layout->last_prepared_rect = NULL;
		}

		rect = prep->rect;

		prep->next = ui->first_free_prepared_rect;
		ui->first_free_prepared_rect = prep;
	}
	else if (layout->flow == Flow_justify)
	{
		// justify stuff
		rect = layout_make_justified_rect(widget_size);
	}
	else
	{
		ui_axis_t axis = ui_flow_axis(layout->flow);

		// cut stuff, maybe insert margins here somehow
		rect2_cut(layout->rect, layout->flow, ui_sz_pix(widget_size.e[axis]), &rect, &layout->rect);
	}

	ui_size_t margin = ui_sz_pix(ui_scalar(UI_SCALAR_WIDGET_MARGIN));
	rect = rect2_cut_margins(rect, margin);

	return rect;
}

rect2_t layout_make_justified_rect(v2_t leaf_dim)
{
	ui_layout_t *layout = ui_get_layout();

	DEBUG_ASSERT_MSG(layout->flow == Flow_justify, "Don't call this function if you don't want a justified rect!");

	float justify_x = layout->justify_x;
	float justify_y = layout->justify_y;

	rect2_t base     = layout->rect;
	v2_t    base_dim = rect2_dim(layout->rect);

	float range_x = MAX(0.0f, base_dim.x - leaf_dim.x);
	float range_y = MAX(0.0f, base_dim.y - leaf_dim.y);

	v2_t min = add(base.min, make_v2(range_x*justify_x, range_y*justify_y));
	v2_t max = add(min, leaf_dim);

	rect2_t result = {
		.min = min,
		.max = max,
	};

	// I'm going to say this fully consumes the parent rectangle
	layout->rect = (rect2_t){0};

	return result;
}

ui_layout_flow_t layout_begin_flow(ui_layout_flow_t flow)
{
	ui_layout_t *layout = ui_get_layout();

	ASSERT_MSG(flow > Flow_none && flow < Flow_COUNT, 
			   "You need to pass a valid flow value! (north/east/south/west)");

	ui_layout_flow_t old_flow = layout->flow;
	layout->flow = flow;

	return old_flow;
}

void layout_end_flow(ui_layout_flow_t old_flow)
{
	ui_layout_t *layout = ui_get_layout();

	ASSERT_MSG(old_flow < Flow_COUNT, 
			   "You need to pass a valid old_flow value! (none/north/east/south/west)");

	layout->flow = old_flow;
}

ui_layout_cut_restore_t layout_begin_cut_internal(const ui_layout_cut_t *cut)
{
	ui_layout_t *layout = ui_get_layout();

	DEBUG_ASSERT_MSG(cut->size.kind > UiSize_none && cut->size.kind < UiSize_COUNT,
					 "You need to pass a valid ui_size to cut anything!");

	//
	// do cut
	//

	rect2_t active, remainder;
	rect2_cut(layout->rect, layout->flow, cut->size, &active, &remainder);

	// "nest" into active rect
	layout->rect = active;

	//
	// handle optional parameters
	//

	ui_layout_flow_t old_flow = layout->flow;

	if (cut->flow != Flow_none)
	{
		layout->flow = cut->flow;
	}

	if (cut->push_clip_rect)
	{
		ui_push_clip_rect(layout->rect);
	}

	if (cut->margin_x.kind != UiSize_none) layout->rect = rect2_cut_margins_horizontally(layout->rect, cut->margin_x);
	if (cut->margin_y.kind != UiSize_none) layout->rect = rect2_cut_margins_vertically  (layout->rect, cut->margin_y);

	layout->justify_x = cut->justify_x;
	layout->justify_y = cut->justify_y;

	//
	// create "restore" data
	//

	ui_layout_cut_restore_t restore = {
		.rect             = remainder,
		.pushed_clip_rect = cut->push_clip_rect,
		.flow             = old_flow,
	};

	return restore;
}

void layout_end_cut_internal(ui_layout_cut_restore_t *restore)
{
	ui_layout_t *layout = ui_get_layout();

	layout->rect         = restore->rect;
	layout->flow         = restore->flow;

	if (restore->pushed_clip_rect)
	{
		ui_pop_clip_rect();
	}

	restore->exit = true;
}
