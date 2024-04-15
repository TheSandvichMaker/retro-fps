#ifndef BVH_H
#define BVH_H

#include "core/api_types.h"

typedef struct bvh_node_t
{
    rect3_t  bounds;    
    uint32_t left_first;  // indices can point into whatever the actual leaf array is for this BVH
    uint16_t count;       
    uint16_t split_axis;   
} bvh_node_t;

typedef struct bvh_iter_t
{
	// "public" iterator data
	bvh_node_t *it_node;
	uint32_t    it_node_index;
	uint32_t    it;

	// iterator state
	bvh_node_t *nodes;
	uint32_t node_count; // used for validation
	uint32_t node_stack[64];
	uint32_t node_stack_at;
} bvh_iter_t;

fn void bvh_iter_init(bvh_iter_t *it, bvh_node_t *nodes, uint32_t node_count);

#endif /* BVH_H */
