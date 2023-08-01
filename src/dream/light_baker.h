#ifndef LIGTABLE_BAKER_H
#define LIGTABLE_BAKER_H

#include "core/api_types.h"

typedef struct lum_params_t
{
    struct map_t *map;

    int ray_count;      // number of diffuse rays per lightmap pixel
    int ray_recursion;  // maximum recursion depth for indirect lighting

    int fogmap_cluster_size;
    int fogmap_scale;
    int fog_light_sample_count;
    float fog_base_scattering;

    struct image_t *skybox;

    bool use_dynamic_sun_shadows;
    v3_t sun_direction;
    v3_t sun_color;
    v3_t sky_color;
} lum_params_t;

typedef struct lum_light_sample_t
{
    float shadow_ray_t;

    v3_t contribution;
    v3_t d;
} lum_light_sample_t;

typedef struct lum_path_vertex_t
{
    struct lum_path_vertex_t *next;
    struct lum_path_vertex_t *prev;

    struct map_brush_t *brush;
    struct map_poly_t *poly;

    lum_light_sample_t *light_samples;

    v3_t contribution;
    v3_t throughput;
    v3_t o;

    unsigned light_sample_count;
} lum_path_vertex_t;

typedef struct lum_path_t
{
    struct lum_path_t *next;

    v2i_t source_pixel;

    v3_t contribution;

    uint32_t vertex_count;
    lum_path_vertex_t *first_vertex;
    lum_path_vertex_t * last_vertex;
} lum_path_t;

// TODO: Lightmap texture debug info to map
// from pixel to path easily
//
// TODO: Switch to flat arrays of data for
// debug data and map_t data, so that I can
// refer to stuff by index instead of having
// a pointer rat's nest, which also makes it
// trivial to map between data.
//
// unsigned poly_to_debug_texture_map[...];
// unsigned debug_texture_to_poly_map[...];

typedef struct lum_debug_data_t
{
    lum_path_t *first_path;
    lum_path_t *last_path;
} lum_debug_data_t;

typedef struct lum_thread_context_t
{
    arena_t          arena;    // 48
    random_series_t  entropy;  // 52
    lum_debug_data_t debug;    // 68
	PAD(60);
} lum_thread_context_t;

typedef struct lum_job_t
{
    lum_thread_context_t    *thread_contexts; // 8
	struct lum_bake_state_t *state;           // 16

    uint32_t brush_index;                     // 20
    uint32_t plane_index;                     // 24
	PAD(48);
} lum_job_t;

typedef struct lum_bake_state_t
{
    alignas(64)
	volatile uint32_t jobs_completed; PAD(60);
    volatile uint32_t cancel; PAD(60);
	uint32_t          job_count;
	uint32_t          thread_count;

	lum_job_t            *jobs;
	lum_thread_context_t *thread_contexts;

	arena_t      arena;
	lum_params_t params;

	hires_time_t start_time;
	hires_time_t end_time;
	double       final_bake_time;

	bool finalized;

	struct
	{
		lum_debug_data_t debug; // TODO: Fix debug data
	} results; // results are only valid if the bake is done and bake_finalize was called and returned true
} lum_bake_state_t;

DREAM_API lum_bake_state_t *bake_lighting     (const lum_params_t *params);
DREAM_API bool              bake_finalize     (lum_bake_state_t *state); // returns true if the bake completed successfully, can be called as much as you want until it returns true
DREAM_API void              bake_cancel       (lum_bake_state_t *state); // will force all remaining jobs to skip and will release the bake state once they all exit
DREAM_API bool              release_bake_state(lum_bake_state_t *state);

DREAM_INLINE bool bake_jobs_completed(lum_bake_state_t *state)
{
	return state->jobs_completed == state->job_count;
}

DREAM_INLINE float bake_progress(lum_bake_state_t *state)
{
	return (float)state->jobs_completed / (float)state->job_count;
}

#endif /* LIGTABLE_BAKER_H */
