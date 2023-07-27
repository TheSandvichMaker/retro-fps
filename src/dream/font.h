#ifndef DREAM_FONT_H
#define DREAM_FONT_H

#include "core/api_types.h"

typedef struct font_glyph_t
{
	uint16_t min_x, min_y;
	uint16_t max_x, max_y;
	float x_offset, y_offset, x_advance;
	float x_offset2, y_offset2;
} font_glyph_t;

typedef struct font_atlas_t
{
	bool initialized;

	arena_t arena;

	float    font_height;
	float    ascent;
	float    descent;
	float    line_gap;
	float    y_advance; // = ascent - descent + line_gap

	uint32_t texture_w;
	uint32_t texture_h;
	resource_handle_t texture;

	font_glyph_t  null_glyph;

	uint32_t      first_glyph;
	uint32_t      glyph_count;
	font_glyph_t *glyph_table; // for the time being, a packed array you directly index into with codepoints
} font_atlas_t;

typedef struct font_range_t
{
	uint32_t start;
	uint32_t end;   // inclusive
} font_range_t;

DREAM_LOCAL font_atlas_t make_font_atlas            (string_t path,      size_t range_count, font_range_t *ranges, float font_size);
DREAM_LOCAL font_atlas_t make_font_atlas_from_memory(string_t font_data, size_t range_count, font_range_t *ranges, float font_size);
DREAM_LOCAL void         destroy_font_atlas         (font_atlas_t *atlas);

DREAM_LOCAL font_glyph_t *atlas_get_glyph  (font_atlas_t *atlas, uint32_t codepoint);
DREAM_LOCAL float         atlas_get_advance(font_atlas_t *atlas, uint32_t a, uint32_t b);

#endif
