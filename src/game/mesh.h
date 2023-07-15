#ifndef DREAM_MESH_H
#define DREAM_MESH_H

#include "core/api_types.h"

typedef struct hull_debug_edge_t
{
	struct hull_debug_edge_t *next;
	edge_t e;
	bool processed_this_step;
} hull_debug_edge_t;

typedef struct hull_debug_triangle_t
{
	struct hull_debug_triangle_t *next;
	triangle_t t;
	bool added_this_step;
} hull_debug_triangle_t;

typedef struct hull_debug_t hull_debug_t;

typedef struct hull_debug_step_t
{
	struct hull_debug_step_t *next;

	hull_debug_t         *debug; // goofy back-reference

	size_t               edge_count;
	hull_debug_edge_t    *first_edge;
	hull_debug_edge_t    *last_edge;

	size_t                triangle_count;
	hull_debug_triangle_t *first_triangle;
	hull_debug_triangle_t *last_triangle;
} hull_debug_step_t;

typedef struct hull_diagnostic_result_t
{
	bool degenerate_hull;
	int  uncontained_point_count;
	int  degenerate_triangle_count;
	int  duplicate_triangle_count;
	int  no_area_triangle_count;

	bool *point_fully_contained;
	bool *triangle_is_degenerate;
	int  *duplicate_triangle_index;
	bool *triangle_has_no_area;
} hull_diagnostic_result_t;

typedef struct hull_debug_t
{
	arena_t arena;

	size_t initial_points_count;
	v3_t  *initial_points;

	size_t           step_count;
	hull_debug_step_t *first_step;
	hull_debug_step_t *last_step;

	hull_diagnostic_result_t *diagnostics;
} hull_debug_t;

DREAM_API hull_debug_step_t *hull_add_debug_step(hull_debug_t *debug);
DREAM_API void hull_add_debug_edge(hull_debug_step_t *step, edge_t e, bool processed_this_step);
DREAM_API void hull_add_debug_triangle(hull_debug_step_t *step, triangle_t t, bool added_this_step);

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
DREAM_API triangle_mesh_t calculate_convex_hull_debug(arena_t *arena, size_t count, v3_t *points, hull_debug_t *debug);
DREAM_API void convex_hull_do_extended_diagnostics   (triangle_mesh_t *mesh, hull_debug_t *debug);

#endif
