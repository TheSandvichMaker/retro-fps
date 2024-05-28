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
	uint32_t string_offset; 
	uint16_t w;             
	uint16_t h;             
	uint16_t d;             
	uint8_t  mip_count;     
	uint8_t  dimensions;    
	uint32_t pixel_format;  
	union { uint64_t data_offset; void *data; };
} pack_texture_t;

typedef struct pack_sound_t
{
	uint32_t string_offset;
	uint32_t sample_count;
	uint32_t channel_count;
	union { uint64_t samples_offset; int16_t *samples; };
} pack_sound_t;

typedef struct map_bvh_node_t map_bvh_node_t;
typedef struct map_entity_t   map_entity_t;
typedef struct map_property_t map_property_t;
typedef struct map_brush_t    map_brush_t;
typedef struct map_plane_t    map_plane_t;
typedef struct map_poly_t     map_poly_t;

typedef struct pack_map_t
{
	rect3_t bounds;

    uint32_t node_count;
    uint32_t entity_count;
    uint32_t property_count;
    uint32_t brush_count;
    uint32_t plane_count;
    uint32_t poly_count;
	uint32_t edge_count;
    uint32_t index_count;
    uint32_t vertex_count;

	union { uint64_t nodes_offset;            map_bvh_node_t *nodes;            };
	union { uint64_t entities_offset;         map_entity_t   *entities;         };
	union { uint64_t properties_offset;       map_property_t *properties;       };
	union { uint64_t brushes_offset;          map_brush_t    *brushes;          };
	union { uint64_t brush_edges_offset;      uint32_t       *brush_edges;      };
	union { uint64_t planes_offset;           map_plane_t    *planes;           };
	union { uint64_t polys_offset;            map_poly_t     *polys;            };
	union { uint64_t indices_offset;          uint32_t       *indices;          };
	union { uint64_t vertex_positions_offset; v3_t           *vertex_positions; };
	union { uint64_t vertex_uvs_offset;       v2_t           *uvs;              };
	union { uint64_t vertex_lm_uvs_offset;    v2_t           *lm_uvs;           };
} pack_map_t;

typedef struct pack_header_t
{
	uint32_t magic;
	uint32_t version;

	uint64_t assets_count;
	uint64_t textures_count;
	uint64_t sounds_count;

	union { uint64_t textures_offset; pack_texture_t *textures; };
	union { uint64_t sounds_offset;   pack_sound_t   *sounds;   };
} pack_header_t;

typedef struct pack_file_t
{
	arena_t        arena;
	pack_header_t *header;
} pack_file_t;

fn void pack_assets(string_t source_directory);
