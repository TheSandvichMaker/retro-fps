#pragma once

typedef enum pack_version_t
{
	PackVer_none = 0,
	PackVer_base = 1,
	PackVer_MAX,
} pack_version_t;

#define PACK_TAG(a, b, c, d) \
	(((uint32_t)a)|((uint32_t)b << 8)|((uint32_t)c << 16)|((uint32_t)d << 24))

typedef enum pack_asset_kind_t
{
	PackAsset_NONE,

	PackAsset_texture,
	PackAsset_sound,

	PackAsset_COUNT,
} pack_asset_kind_t;

typedef struct pack_texture_t
{
	uint32_t string_offset; // 4
	uint16_t w;             // 6
	uint16_t h;             // 8
	uint16_t d;             // 10
	uint8_t  mip_count;     // 11
	uint8_t  dimensions;    // 12
	uint32_t pixel_format;  // 16
	uint64_t data_offset;   // 20
} pack_texture_t;

typedef struct pack_sound_t
{
	uint32_t string_offset;
	uint32_t sample_count;
	uint32_t channel_count;
	uint64_t data_offset;
} pack_sound_t;

typedef struct pack_header_t
{
	uint32_t magic;
	uint32_t version;

	uint64_t assets_count;

	uint64_t strings_offset;

	uint64_t textures_count;
	uint64_t textures_offset;

	uint64_t sounds_count;
	uint64_t sounds_offset;
} pack_header_t;

typedef struct pack_file_t
{
	arena_t        arena;
	pack_header_t *header;
} pack_file_t;

fn void pack_assets(string_t source_directory);
