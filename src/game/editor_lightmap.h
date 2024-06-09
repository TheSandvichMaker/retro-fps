#pragma once

typedef struct editor_window_t editor_window_t;

typedef struct editor_lightmap_state_t
{
    bool debug_lightmaps;

    map_brush_t *selected_brush;
    map_plane_t *selected_plane;
    map_poly_t  *selected_poly;

    bool pixel_selection_active;
    rect2i_t selected_pixels;

    bool show_direct_light_rays;
    bool show_indirect_light_rays;

    bool fullbright_rays;
    bool no_ray_depth_test;

    int min_display_recursion;
    int max_display_recursion;
} editor_lightmap_state_t;

fn_local void editor_init_lightmap_stuff             (editor_lightmap_state_t *state);
fn_local void editor_do_lightmap_window              (editor_lightmap_state_t *state, editor_window_t *window, rect2_t rect);
fn_local void editor_update_and_render_lightmap_stuff(editor_lightmap_state_t *state, float dt);
