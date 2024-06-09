// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#define STBRP_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STBTT_malloc(size, userdata) m_alloc_nozero((arena_t *)userdata, size, 16)
#define STBTT_free(pointer, userdata) ((void)pointer, (void)userdata, 0)

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

font_t *make_font(string_t path, size_t range_count, font_range_t *ranges, float font_size)
{
	font_t *result = NULL;

	m_scoped_temp
	{
		string_t font_data = fs_read_entire_file(temp, path);
		if (font_data.count)
		{
			result = make_font_from_memory(path, font_data, range_count, ranges, font_size);
		}
	}

	return result;
}

font_t *make_font_from_memory(string_t cosmetic_name, string_t font_data, size_t range_count, font_range_t *ranges, float font_size)
{
	font_t *result = NULL;

	if (NEVER(font_data.count == 0))
	{
		// TODO: Fallback font
		return result;
	}

	result = m_bootstrap(font_t, arena);
	result->cosmetic_name = m_copy_string(&result->arena, cosmetic_name);

	m_scoped_temp
	{
		unsigned char *data = (unsigned char *)font_data.data;

		// TODO: Think about bitmap size
		int w = result->texture_w = 512;
		int h = result->texture_h = 512;

		unsigned char *pixels = m_alloc_nozero(temp, w*h, 16);

		stbtt_fontinfo info;
		if (stbtt_InitFont(&info, data, 0))
		{
			float font_scale = stbtt_ScaleForPixelHeight(&info, font_size);

			int ascent, descent, line_gap;
			stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

			result->oversampling_x = 2;
			result->oversampling_y = 2;
			result->height      = font_size;
			result->ascent      = font_scale*(float)ascent;
			result->descent     = font_scale*(float)descent;
			result->line_gap    = font_scale*(float)line_gap;
			result->y_advance   = result->ascent - result->descent + result->line_gap;

			stbtt_pack_context spc;
			if (stbtt_PackBegin(&spc, pixels, w, h, w, 1, temp))
			{
				stbtt_PackSetOversampling(&spc, result->oversampling_x, result->oversampling_y);

				// translate passed in font ranges to stbtt font ranges

				stbtt_pack_range *tt_ranges = m_alloc_array(temp, range_count, stbtt_pack_range);

				uint32_t first_codepoint = UINT32_MAX;
				uint32_t  last_codepoint = 0;
				for (size_t i = 0; i < range_count; i++)
				{
					font_range_t     *my_range = &ranges   [i];
					stbtt_pack_range *tt_range = &tt_ranges[i];

					if (first_codepoint > my_range->start)
						first_codepoint = my_range->start;

					if (last_codepoint < my_range->end)
						last_codepoint = my_range->end;

					tt_range->font_size                        = font_size;
					tt_range->first_unicode_codepoint_in_range = (int)my_range->start;
					tt_range->num_chars                        = (int)(my_range->end - my_range->start + 1);
					tt_range->chardata_for_range               = m_alloc_array_nozero(temp, tt_range->num_chars, stbtt_packedchar);
				}

				if (stbtt_PackFontRanges(&spc, data, 0, tt_ranges, (int)range_count))
				{
					result->first_glyph = first_codepoint;
					result->glyph_count = last_codepoint - first_codepoint + 1;
					result->glyph_table = m_alloc_array(&result->arena, result->glyph_count, font_glyph_t);

					for (size_t range_index = 0; range_index < range_count; range_index++)
					{
						stbtt_pack_range *tt_range = &tt_ranges[range_index];

						for (int packed_index = 0; packed_index < tt_range->num_chars; packed_index++)
						{
							uint32_t glyph_index = tt_range->first_unicode_codepoint_in_range + packed_index - result->first_glyph;

							stbtt_packedchar *packed   = &tt_range->chardata_for_range[packed_index];
							font_glyph_t     *my_glyph = &result->glyph_table[glyph_index];

							my_glyph->min_x     =  packed->x0;
							my_glyph->min_y     =  packed->y0;
							my_glyph->max_x     =  packed->x1;
							my_glyph->max_y     =  packed->y1;
							my_glyph->x_offset  =  packed->xoff;
							my_glyph->y_offset  = -packed->yoff2; // swapping yoff2 and yoff because we're in a Y-is-up scenario while stbtt assumes Y-is-down
							my_glyph->x_advance =  packed->xadvance;
							my_glyph->x_offset2 =  packed->xoff2;
							my_glyph->y_offset2 = -packed->yoff;  // swapping yoff2 and yoff because we're in a Y-is-up scenario while stbtt assumes Y-is-down
						}
					}
				}
				else
				{
					// TODO: Log failure
				}

				stbtt_PackEnd(&spc);
			}
		}
		else
		{
			// TODO: Log failure
		}

		result->texture = rhi_create_texture(&(rhi_create_texture_params_t){
			.debug_name = cosmetic_name,
			.dimension  = RhiTextureDimension_2d,
			.format     = PixelFormat_r8_unorm,
			.width      = w,
			.height     = h,
			.initial_data = &(rhi_texture_data_t){
				.subresources      = &pixels,
				.subresource_count = 1,
				.row_stride        = sizeof(uint8_t)*w,
			},
		});

		result->initialized = true;
	}

	return result;
}

void destroy_font(font_t *font)
{
	rhi_destroy_texture(font->texture);
	m_release(&font->arena);
}

font_glyph_t *font_get_glyph(font_t *font, uint32_t codepoint)
{
	font_glyph_t *result = &font->null_glyph;
	if (codepoint >= font->first_glyph && codepoint < font->first_glyph + font->glyph_count)
	{
		result = &font->glyph_table[codepoint - font->first_glyph];
	}
	return result;
}

float font_get_advance(font_t *font, uint32_t a, uint32_t b)
{
	(void)b;
	// TODO: Kerning
	font_glyph_t *glyph = font_get_glyph(font, a);
	return glyph->x_advance;
}

prepared_glyphs_t font_prepare_glyphs(font_t *font, arena_t *arena, string_t text)
{
	prepared_glyphs_t result = {0};
	result.count  = text.count;
	result.glyphs = m_alloc_array_nozero(arena, text.count, prepared_glyph_t);

	float w = (float)font->texture_w;
	float h = (float)font->texture_h;

	v2_t at = {0};
	at.y -= 0.5f*font->descent;

	at.y = roundf(at.y);

	for (size_t i = 0; i < text.count; i++)
	{
		char c = text.data[i];

		if (is_newline(c))
		{
			at.x  = 0.0f;
			at.y -= font->y_advance;
		}
		else
		{
			font_glyph_t *glyph = font_get_glyph(font, c);

			float cw = (float)(glyph->max_x - glyph->min_x);
			float ch = (float)(glyph->max_y - glyph->min_y);

			cw /= (float)font->oversampling_x;
			ch /= (float)font->oversampling_y;

			v2_t point = at;
			point.x += glyph->x_offset;
			point.y += glyph->y_offset;

			prepared_glyph_t *prep = &result.glyphs[i];
			prep->rect = rect2_from_min_dim(point, make_v2(cw, ch));
			prep->rect_uv = (rect2_t){
				.x0 = (float)glyph->min_x / w,
				.y0 = (float)glyph->max_y / h,
				.x1 = (float)glyph->max_x / w,
				.y1 = (float)glyph->min_y / h,
			};
			prep->byte_offset = (uint32_t)i;

			result.bounds = rect2_union(result.bounds, prep->rect);

			if (i + 1 < text.count)
			{
				char c_next = text.data[i + 1];
				at.x += font_get_advance(font, c, c_next);
			}
		}
	}

	return result;
}
