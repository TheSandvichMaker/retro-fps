#include "bvh.h"

void bvh_iter_init(bvh_iter_t *it, bvh_node_t *nodes, uint32_t node_count)
{
	it->nodes         = nodes;
	it->node_count    = node_count;
	it->node_stack_at = 1;
	it->node_stack[0] = 0;
}

#if 0
bool bvh_iter_valid(bvh_iter_t *it)
{
	return it->node_stack_at > 0;
}

void bvh_iter_next(
#endif