#include "core/common.h"
#include "core/math.h"
#include "core/arena.h"
#include "core/stretchy_buffer.h"

#include "game/mesh.h"

//
// gift-wrapping convex hull
// https://www.cs.jhu.edu/~misha/Spring16/09.pdf
//

ch_debug_step_t *ch_add_debug_step(ch_debug_t *debug)
{
	debug->step_count++;

	ch_debug_step_t *result = m_alloc_struct(&debug->arena, ch_debug_step_t);
	sll_push_back(debug->first_step, debug->last_step, result);

	result->debug = debug;

	return result;
}

void ch_add_debug_edge(ch_debug_step_t *step, edge_t e, bool processed_this_step)
{
	ch_debug_edge_t *debug_edge = m_alloc_struct(&step->debug->arena, ch_debug_edge_t);
	debug_edge->e = e;
	debug_edge->processed_this_step = processed_this_step;

	step->edge_count++;
	sll_push_back(step->first_edge, step->last_edge, debug_edge);
}

void ch_add_debug_triangle(ch_debug_step_t *step, triangle_t t, bool added_this_step)
{
	ch_debug_triangle_t *debug_triangle = m_alloc_struct(&step->debug->arena, ch_debug_triangle_t);
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
	ASSERT(count > 0);

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

	v3_t q = p;

	for (size_t i = 0; i < count; i++)
	{
		v3_t r = points[i];

		// I don't get why you would do this
		if (q.z == r.z && q.y == r.y && r.x > q.x)
			q = r;
	}

	if (q.x == p.x && q.y == p.y && q.z == p.z)
	{
		// I guess this is a valid thing to do?
		q = add(p, make_v3(1, 0, 0));
	}

	q = triangle_vertex_with_all_points_to_the_left((edge_t){p, q}, count, points);
	return (edge_t){p, q};
}

triangle_mesh_t calculate_convex_hull(arena_t *arena, size_t count, v3_t *points)
{
	return calculate_convex_hull_debug(arena, count, points, NULL);
}

triangle_mesh_t calculate_convex_hull_debug(arena_t *arena, size_t count, v3_t *points, ch_debug_t *debug)
{
	ASSERT(arena != temp);

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

	m_scoped(temp)
	{
		stretchy_buffer(edge_t)     Q = NULL;
		stretchy_buffer(triangle_t) H = NULL;

		sb_push(Q, find_edge_on_hull(count, points));

		stretchy_buffer(edge_t) processed = NULL; // IDIOCY

		size_t step_index = 0;
		while (sb_count(Q) > 0)
		{
			edge_t e = sb_pop(Q);

			bool is_processed = false;

			if (step_index == 8)
			{
				int y = 0;
				(void)y;
			}

			// MORONICITY
			for (int64_t i = (int64_t)sb_count(processed) - 1; i >= 0; i--)
			{
				edge_t e_prime = processed[i];

				if ((v3_equal_exact(e.a, e_prime.a) && v3_equal_exact(e.b, e_prime.b)) ||
					(v3_equal_exact(e.a, e_prime.b) && v3_equal_exact(e.b, e_prime.a)))
				{
					is_processed = true;
					break;
				}
			}

			if (is_processed)
				continue;

			v3_t       q = triangle_vertex_with_all_points_to_the_left(e, count, points);
			triangle_t t = (triangle_t){e.a, e.b, q};

			if (triangle_area_sq(t.a, t.b, t.c) > 0.0001f)
			{
				if (debug)
				{
					ch_debug_step_t *step = ch_add_debug_step(debug);

					ch_add_debug_triangle(step, t, true);
					for (size_t i = 0; i < sb_count(H); i++)
					{
						ch_add_debug_triangle(step, H[i], false);
					}

					ch_add_debug_edge(step, e, true);
					for (size_t i = 0; i < sb_count(Q); i++)
					{
						ch_add_debug_edge(step, Q[i], false);
					}
				}

				sb_push(H, t);

				sb_push(Q, ((edge_t){t.c, t.b}));
				sb_push(Q, ((edge_t){t.a, t.c}));

				sb_push(processed, e);

				step_index++;
			}
		}

		result.triangle_count = sb_count(H);
		result.triangles      = sb_copy(arena, H);
	}

	return result;
}


