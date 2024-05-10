// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#ifndef DREAM_FONT_H
#define DREAM_FONT_H

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

	int oversampling_x;
	int oversampling_y;

	float    font_height;
	float    ascent;
	float    descent;
	float    line_gap;
	float    y_advance; // = ascent - descent + line_gap

	uint32_t texture_w;
	uint32_t texture_h;
	texture_handle_t texture;

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

typedef struct prepared_glyph_t
{
	rect2_t  rect;
	rect2_t  rect_uv;
	uint32_t byte_offset;
} prepared_glyph_t;

typedef struct prepared_glyphs_t
{
	size_t            count;
	prepared_glyph_t *glyphs;
	rect2_t           bounds;
} prepared_glyphs_t;

fn font_atlas_t make_font_atlas            (string_t path,      size_t range_count, font_range_t *ranges, float font_size);
fn font_atlas_t make_font_atlas_from_memory(string_t font_data, size_t range_count, font_range_t *ranges, float font_size);
fn void         destroy_font_atlas         (font_atlas_t *atlas);

fn font_glyph_t     *atlas_get_glyph           (font_atlas_t *atlas, uint32_t codepoint);
fn font_glyph_t     *atlas_get_glyph_from_index(font_atlas_t *atlas, uint32_t index);
fn float             atlas_get_advance         (font_atlas_t *atlas, uint32_t a, uint32_t b);
fn prepared_glyphs_t atlas_prepare_glyphs      (font_atlas_t *atlas, arena_t *arena, string_t text);

#endif