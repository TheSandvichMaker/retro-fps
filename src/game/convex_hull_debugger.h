#ifndef DREAM_CONVEX_HULL_DEBUGGER_H
#define DREAM_CONVEX_HULL_DEBUGGER_H

#include "core/api_types.h"

#include "ui.h"
#include "mesh.h"

typedef struct convex_hull_debugger_t
{
	ui_window_t window;

	bool initialized;

	int random_seed;
	int point_count;
	bool automatically_recalculate_hull;

	arena_t         mesh_arena;
	triangle_mesh_t mesh;
	hull_debug_t    debug;

	bool   show_points;
	bool   show_processed_edge;
	bool   show_non_processed_edges;
	bool   show_new_triangle;
	bool   show_triangles;
	bool   show_duplicate_triangles;
	bool   show_wireframe;
	int    current_step_index;
	int    test_tetrahedron_index;

	bool   brute_force_test;
	int    brute_force_min_point_count;
	int    brute_force_max_point_count;
	int    brute_force_iterations_per_point_count;
	float  brute_force_triggered_success_timer;
	float  brute_force_triggered_error_timer;
} convex_hull_debugger_t;

fn void convex_hull_debugger_window_proc(struct ui_window_t *);
fn void convex_hull_debugger_update_and_render(convex_hull_debugger_t *debugger, struct r_context_t *rc, r_view_index_t game_view);

#endif
