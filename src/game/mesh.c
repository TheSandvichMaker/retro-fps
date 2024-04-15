#include "core/common.h"
#include "core/math.h"
#include "core/arena.h"
#include "core/stretchy_buffer.h"

#include "mesh.h"

//
// gift-wrapping convex hull
// https://www.cs.jhu.edu/~misha/Spring16/09.pdf
//

hull_debug_step_t *hull_add_debug_step(hull_debug_t *debug)
{
	debug->step_count++;

	hull_debug_step_t *result = m_alloc_struct(&debug->arena, hull_debug_step_t);
	sll_push_back(debug->first_step, debug->last_step, result);

	result->debug = debug;

	return result;
}

void hull_add_debug_edge(hull_debug_step_t *step, edge_t e, bool processed_this_step)
{
	hull_debug_edge_t *debug_edge = m_alloc_struct(&step->debug->arena, hull_debug_edge_t);
	debug_edge->e = e;
	debug_edge->processed_this_step = processed_this_step;

	step->edge_count++;
	sll_push_back(step->first_edge, step->last_edge, debug_edge);
}

void hull_add_debug_triangle(hull_debug_step_t *step, triangle_t t, bool added_this_step)
{
	hull_debug_triangle_t *debug_triangle = m_alloc_struct(&step->debug->arena, hull_debug_triangle_t);
	debug_triangle->t = t;
	debug_triangle->added_this_step = added_this_step;

	step->triangle_count++;
	sll_push_back(step->first_triangle, step->last_triangle, debug_triangle);
}

DREAM_INLINE v3_t triangle_vertex_with_all_points_to_the_left(edge_t e, size_t count, v3_t *points)
{
	ASSERT(count > 0);

	v3_t p = points[0];

	float area_sq = triangle_area_sq(e.a, e.b, p);

	for (size_t i = 1; i < count; i++)
	{
		v3_t p_prime = points[i];

		if (v3_equal_exact(p_prime, e.a) ||
			v3_equal_exact(p_prime, e.b))
		{
			continue;
		}

		float volume = tetrahedron_signed_volume(e.a, e.b, p, p_prime);

		if (volume > 0.0f)
		{
			p = p_prime;
		}
		else if (volume == 0.0f)
		{
			float area_sq_prime = triangle_area_sq(e.a, e.b, p_prime);

			if (area_sq_prime > area_sq)
			{
				p       = p_prime;
				area_sq = area_sq_prime;
			}
		}
	}

	return p;
}

DREAM_INLINE edge_t find_edge_on_hull(size_t count, v3_t *points)
{
	ASSERT(count > 1);

	v3_t p = points[0];

	for (size_t i = 1; i < count; i++)
	{
		v3_t p_prime = points[i];

		if (p.x > p_prime.x)
			p = p_prime;
		else if (p.x == p_prime.x && p.y > p_prime.y)
			p = p_prime;
		else if (p.x == p_prime.x && p.y == p_prime.y && p.z > p_prime.z)
			p = p_prime;
	}

	v3_t q = add(p, make_v3(0, 0, 1));

	q = triangle_vertex_with_all_points_to_the_left((edge_t){p, q}, count, points);
	return (edge_t){p, q};
}

triangle_mesh_t calculate_convex_hull(arena_t *arena, size_t count, v3_t *points)
{
	return calculate_convex_hull_debug(arena, count, points, NULL);
}

DREAM_INLINE bool triangles_share_all_points(triangle_t t, triangle_t t2)
{
	bool result = false;

	// This is also very goofy. If triangles used indices I'd just sort the indices.
	stack_t(v3_t, 3) verts = {0};
	stack_push(verts, t.a);
	stack_push(verts, t.b);
	stack_push(verts, t.c);

	bool a_equal = false;
	for (size_t i = 0; i < stack_count(verts); i++)
	{
		if (v3_equal_exact(t2.a, verts.values[i]))
		{
			a_equal = true;
			stack_unordered_remove(verts, i);
			break;
		}
	}

	bool b_equal = false;
	for (size_t i = 0; i < stack_count(verts); i++)
	{
		if (v3_equal_exact(t2.b, verts.values[i]))
		{
			b_equal = true;
			stack_unordered_remove(verts, i);
			break;
		}
	}

	bool c_equal = false;
	for (size_t i = 0; i < stack_count(verts); i++)
	{
		if (v3_equal_exact(t2.c, verts.values[i]))
		{
			c_equal = true;
			stack_unordered_remove(verts, i);
			break;
		}
	}

	if (a_equal && b_equal && c_equal)
	{
		result = true;
	}

	return result;
}

DREAM_INLINE bool triangle_in_set(const stretchy_buffer(triangle_t) H, triangle_t t)
{
	// TODO: Fix this N^2 bull

	bool result = false;

	for (int64_t triangle_index = (int64_t)sb_count(H) - 1; triangle_index >= 0; triangle_index--)
	{
		triangle_t t2 = H[triangle_index];
		if (triangles_share_all_points(t, t2))
		{
			result = true;
			break;
		}
	}

	return result;
}

DREAM_INLINE bool edge_in_set(const stretchy_buffer(edge_t) Q, edge_t e)
{
	// TODO: Fix this N^2 bull

	bool result = false;

	for (int64_t i = (int64_t)sb_count(Q) - 1; i >= 0; i--)
	{
		edge_t e_prime = Q[i];

		if ((v3_equal_exact(e.a, e_prime.a) && v3_equal_exact(e.b, e_prime.b)) ||
			(v3_equal_exact(e.a, e_prime.b) && v3_equal_exact(e.b, e_prime.a)))
		{
			result = true;
			break;
		}
	}

	return result;
}

triangle_mesh_t calculate_convex_hull_debug(arena_t *arena, size_t count, v3_t *points, hull_debug_t *debug)
{
	arena_t *temp = m_get_temp(&arena, 1);
	m_scope_begin(temp);

	ASSERT(count > 3);

	// TODO FIXME: A lot of horrible O(N^2) stuff happening here to check if things are in a set
	// TODO FIXME: A lot of horrible O(N^2) stuff happening here to check if things are in a set
	// TODO FIXME: A lot of horrible O(N^2) stuff happening here to check if things are in a set

	if (debug)
	{
		if (debug->step_count > 0)
		{
			m_release(&debug->arena);
		}

		zero_struct(debug);
		debug->initial_points_count = count;
		debug->initial_points       = m_copy_array(&debug->arena, points, count);
	}

	triangle_mesh_t result = {0};

	m_scoped_temp
	{
		// TODO: Pre-allocate upper bound as given by Euler's formula
		// V = N
		// E = 3N - 6
		// F = 2N - 4
		// V - E + F = 2
		stretchy_buffer(edge_t)     Q = NULL;
		stretchy_buffer(triangle_t) H = NULL;

		sb_push(Q, find_edge_on_hull(count, points));

		stretchy_buffer(edge_t) processed = NULL;

		size_t step_index = 0;
		while (sb_count(Q) > 0)
		{
			edge_t e = sb_pop(Q);

			if (edge_in_set(processed, e))
				continue;

			v3_t       q = triangle_vertex_with_all_points_to_the_left(e, count, points);
			triangle_t t = (triangle_t){e.a, e.b, q};

			if (debug)
			{
				hull_debug_step_t *step = hull_add_debug_step(debug);

				for (size_t i = 0; i < sb_count(H); i++)
				{
					hull_add_debug_triangle(step, H[i], false);
				}
				hull_add_debug_triangle(step, t, true);

				hull_add_debug_edge(step, e, true);
				for (size_t i = 0; i < sb_count(Q); i++)
				{
					hull_add_debug_edge(step, Q[i], false);
				}
			}

			if (!triangle_in_set(H, t)) sb_push(H, t);

			edge_t e1 = { t.c, t.b };
			edge_t e2 = { t.a, t.c };

			if (!edge_in_set(Q, e1)) sb_push(Q, e1);
			if (!edge_in_set(Q, e2)) sb_push(Q, e2);

			sb_push(processed, e);

			step_index++;
		}

		result.triangle_count = sb_count(H);
		result.triangles      = sb_copy(arena, H);
	}

	m_scope_end(temp);

	return result;
}

DREAM_LOCAL void convex_hull_do_extended_diagnostics(triangle_mesh_t *mesh, hull_debug_t *debug)
{
	if (NEVER(!mesh))  return;
	if (NEVER(!debug)) return;

	if (ALWAYS(!debug->diagnostics))
	{
		hull_diagnostic_result_t *diagnostics = m_alloc_struct(&debug->arena, hull_diagnostic_result_t);
		debug->diagnostics = diagnostics;

		bool degenerate_hull           = false;
		int  degenerate_triangle_count = 0;
		int  duplicate_triangle_count  = 0;
		int  no_area_triangle_count    = 0;

		bool *point_fully_contained    = m_alloc_array_nozero(&debug->arena, debug->initial_points_count, bool);
		bool *triangle_is_degenerate   = m_alloc_array_nozero(&debug->arena, mesh->triangle_count, bool);
		int  *duplicate_triangle_index = m_alloc_array_nozero(&debug->arena, mesh->triangle_count, int);
		bool *triangle_has_no_area     = m_alloc_array       (&debug->arena, mesh->triangle_count, bool);

		for (size_t i = 0; i < debug->initial_points_count; i++)
		{
			point_fully_contained[i] = true;
		}

		for (size_t i = 0; i < mesh->triangle_count; i++)
		{
			duplicate_triangle_index[i] = -1;
		}

		if (debug->initial_points_count > 0 && mesh->triangle_count > 0)
		{
			for (size_t triangle_index = 0; triangle_index < mesh->triangle_count; triangle_index++)
			{
				triangle_t t = mesh->triangles[triangle_index];

				bool degenerate_triangle = false;

				float area = triangle_area_sq(t.a, t.b, t.c);
				if (area < 0.00001f)
				{
					degenerate_triangle = true;
					triangle_has_no_area[triangle_index] = true;
					no_area_triangle_count += 1;
				}

				if (!degenerate_triangle)
				{
					for (size_t point_index = 0; point_index < debug->initial_points_count; point_index++)
					{
						v3_t p = debug->initial_points[point_index];

						if (!v3_equal_exact(p, t.a) &&
							!v3_equal_exact(p, t.b) &&
							!v3_equal_exact(p, t.c))
						{
							float volume = tetrahedron_signed_volume(t.a, t.b, t.c, p);

							if (volume > 0.0f)
							{
								degenerate_triangle = true;
								point_fully_contained[point_index] = false;
							}
						}
					}
				}

				if (degenerate_triangle)
				{
					degenerate_triangle_count += 1;
					degenerate_hull = true;
				}

				triangle_is_degenerate[triangle_index] = degenerate_triangle;

				if (duplicate_triangle_index[triangle_index] == -1)
				{
					for (size_t test_triangle_index = 0; test_triangle_index < mesh->triangle_count; test_triangle_index++)
					{
						if (test_triangle_index == triangle_index)
							continue;

						triangle_t t2 = mesh->triangles[test_triangle_index];

						if (triangles_share_all_points(t, t2))
						{
							duplicate_triangle_index[triangle_index]      = (int)test_triangle_index;
							duplicate_triangle_index[test_triangle_index] = (int)triangle_index;
							duplicate_triangle_count += 1;
						}
					}
				}
			}
		}

		int uncontained_point_count = 0;
		for (size_t i = 0; i < debug->initial_points_count; i++)
		{
			if (!point_fully_contained[i])
			{
				uncontained_point_count += 1;
			}
		}

		diagnostics->degenerate_hull           = degenerate_hull;
		diagnostics->uncontained_point_count   = uncontained_point_count;
		diagnostics->degenerate_triangle_count = degenerate_triangle_count;
		diagnostics->duplicate_triangle_count  = duplicate_triangle_count; 
		diagnostics->no_area_triangle_count    = no_area_triangle_count;
		diagnostics->point_fully_contained     = point_fully_contained;
		diagnostics->triangle_is_degenerate    = triangle_is_degenerate;
		diagnostics->duplicate_triangle_index  = duplicate_triangle_index;
		diagnostics->triangle_has_no_area      = triangle_has_no_area;
	}
}
