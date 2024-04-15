// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef struct render_settings_t
{
    int render_w, render_h;
    int msaa_level;
    int rendertarget_precision; // 0 = default, 1 = high precision

    int sun_shadowmap_resolution;
    bool use_dynamic_sun_shadows;

    uint32_t version;
} render_settings_t;

typedef struct bitmap_font_t
{
    unsigned w, h, cw, ch;
    texture_handle_t texture;
} bitmap_font_t;

extern v3_t g_debug_colors[6];

static inline v3_t r_debug_color(size_t i)
{
    return g_debug_colors[i % ARRAY_COUNT(g_debug_colors)];
}

typedef struct r_command_buffer_t r_command_buffer_t;

typedef struct r_context_t
{
    r_command_buffer_t *commands;

#if DREAM_SLOW
    string_list_t identifier_stack;
    string_t      next_command_identifier;
#endif

    r_immediate_t *current_immediate;

    uint32_t current_first_ui_rect;
    uint32_t current_ui_rect_count;
    texture_handle_t current_ui_texture;

    stack_t(r_screen_layer_t, 8) screen_layers_stack;
    stack_t(r_view_index_t,  64) views_stack;
    stack_t(r_view_layer_t,   8) view_layers_stack;
    
    r_view_index_t screenspace; // REMOVE...
} r_context_t;

fn void           r_init_render_context(r_context_t *rc, r_command_buffer_t *commands);
fn void           r_push_command_identifier(r_context_t *rc, string_t info);
fn void           r_pop_command_identifier (r_context_t *rc);

#define R_COMMAND_IDENTIFIER(rc, ident) \
    DEFER_LOOP(r_push_command_identifier(rc, ident), r_pop_command_identifier(rc))

#define R_COMMAND_IDENTIFIER_FUNC(rc) R_COMMAND_IDENTIFIER(rc, string_from_cstr(__FUNCTION__))
// NOTE: I am treating __FUNCTION__ as a runtime string because I think in MSVC it is. Cringe.
#define R_COMMAND_IDENTIFIER_LOC(rc, info) R_COMMAND_IDENTIFIER(rc, Sf(LOC_CSTRING ":%s: %.*s", __FUNCTION__, Sx(info)))

fn void           r_draw_mesh(r_context_t *rc, m4x4_t transform, mesh_handle_t mesh, const r_material_t *material);
fn void           r_push_command        (r_context_t *rc, r_command_t command);
fn void          *r_allocate_command_data(r_context_t *rc, size_t size);

fn r_immediate_t *r_immediate_begin     (r_context_t *rc);
fn uint32_t       r_immediate_vertex    (r_context_t *rc, r_vertex_immediate_t vertex);
fn void           r_immediate_index     (r_context_t *rc, uint32_t index);
fn void           r_immediate_end       (r_context_t *rc, r_immediate_t *imm);

#define R_IMMEDIATE(rc, imm) for (r_immediate_t *imm = r_immediate_begin(rc); \
                                  imm;                                        \
                                  r_immediate_end(rc, imm), imm = NULL)

fn void           r_ui_rect             (r_context_t *rc, r_ui_rect_t rect);
fn void           r_ui_texture          (r_context_t *rc, texture_handle_t texture);
fn void           r_flush_ui_rects      (r_context_t *rc); // frowny?

fn r_view_index_t r_make_view           (r_context_t *rc, const r_view_t *view);
fn void           r_push_view           (r_context_t *rc, r_view_index_t view);
fn void           r_pop_view            (r_context_t *rc);
fn r_view_t      *r_get_view            (r_context_t *rc); // gets current view
fn r_view_t      *r_get_view_from_index (r_context_t *rc, r_view_index_t index);
fn v3_t           r_to_view_space       (const r_view_t *view, v3_t p, float w);
fn void           r_push_view_layer     (r_context_t *rc, r_view_layer_t layer);
fn void           r_pop_view_layer      (r_context_t *rc);

#define R_VIEW(rc, view_index)  DEFER_LOOP(r_push_view(rc, view_index), r_pop_view(rc))
#define R_VIEW_LAYER(rc, layer) DEFER_LOOP(r_push_view_layer(rc, layer), r_pop_view_layer(rc))

fn void r_push_layer(r_context_t *rc, r_screen_layer_t layer);
fn void r_pop_layer(r_context_t *rc);

#define R_LAYER(rc, layer) DEFER_LOOP(r_push_layer(rc, layer), r_pop_layer(rc))
