#ifndef DREAM_MESH_H
#define DREAM_MESH_H

#include "core/api_types.h"

typedef struct ch_debug_edge_t
{
	struct ch_debug_edge_t *next;
	edge_t e;
	bool processed_this_step;
} ch_debug_edge_t;

typedef struct ch_debug_triangle_t
{
	struct ch_debug_triangle_t *next;
	triangle_t t;
	bool added_this_step;
} ch_debug_triangle_t;

typedef struct ch_debug_t ch_debug_t;

typedef struct ch_debug_step_t
{
	struct ch_debug_step_t *next;

	ch_debug_t          *debug; // goofy back-reference

	size_t               edge_count;
	ch_debug_edge_t     *first_edge;
	ch_debug_edge_t     * last_edge;

	size_t               triangle_count;
	ch_debug_triangle_t *first_triangle;
	ch_debug_triangle_t * last_triangle;
} ch_debug_step_t;

typedef struct ch_debug_t
{
	arena_t arena;

	size_t initial_points_count;
	v3_t  *initial_points;

	size_t           step_count;
	ch_debug_step_t *first_step;
	ch_debug_step_t * last_step;
} ch_debug_t;

DREAM_API ch_debug_step_t *ch_add_debug_step(ch_debug_t *debug);
DREAM_API void ch_add_debug_edge(ch_debug_step_t *step, edge_t e, bool processed_this_step);
DREAM_API void ch_add_debug_triangle(ch_debug_step_t *step, triangle_t t, bool added_this_step);

typedef struct triangle_mesh_t
{
	size_t triangle_count;

	union
	{
		triangle_t *triangles;
		v3_t       *points;
	};
} triangle_mesh_t;

DREAM_API triangle_mesh_t calculate_convex_hull      (arena_t *arena, size_t count, v3_t *points);
DREAM_API triangle_mesh_t calculate_convex_hull_debug(arena_t *arena, size_t count, v3_t *points, ch_debug_t *debug);

#endif
