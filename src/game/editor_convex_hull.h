// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct editor_window_t editor_window_t;

typedef struct editor_convex_hull_debugger_t
{
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
} editor_convex_hull_debugger_t;

fn void editor_init_convex_hull_debugger    (editor_convex_hull_debugger_t *debugger);
fn void editor_do_convex_hull_window        (editor_convex_hull_debugger_t *debugger, editor_window_t *window);
fn void editor_convex_hull_update_and_render(editor_convex_hull_debugger_t *debugger);
