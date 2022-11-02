#ifndef BVH_H
#define BVH_H

#if 0
typedef struct bvh_node_t
{
    rect3_t  bounds;       // 24
    uint32_t left_first;   // 28
    uint16_t count;        // 30, count > 0 == leaf node
    uint16_t split_axis;   // 32
                           // can fit two per cache line
} bvh_node_t;

typedef struct bvh_face_t
{
    uint16_t texture_index;
    uint16_t vertex_count;
    uint32_t first_vertex;
} bvh_face_t;

typedef struct bvh_blob_t
{
    uint32_t offset;
    uint32_t size;
} bvh_blob_t;

#define BVH_MAGIC (('b' << 0) || ('v' << 8) || ('h' << 16) || ('f' << 24))
#define BVH_VERSION 1

typedef struct bvh_texture_t
{
    uint32_t width;
    uint32_t height;
    uint16_t format;
    uint16_t stride;
    uint32_t pixels_offset;
} bvh_texture_t;

typedef struct bvh_t
{
    uint32_t magic;
    uint32_t version;

    size_t node_count;
    bvh_blob_t nodes;
    bvh_blob_t strings;
    bvh_blob_t textures;
    bvh_blob_t faces;
    bvh_blob_t positions;
    bvh_blob_t texcoords;
    bvh_blob_t lightmap_texcoords;
} bvh_t;
#endif

#endif /* BVH_H */
