#include <stdlib.h>
#include <float.h>

//
//
//

#include "map.h"
#include "asset.h"
#include "render/render.h"
#include "core/subset_iter.h"
#include "core/bulk_data.h"
#include "core/sort.h"
#include "core/hashtable.h"
#include "render/diagram.h"

//
// .map parser
//

typedef enum map_token_kind_e
{
    MTOK_TEXT = 128,
    MTOK_STRING,
    MTOK_OPERATOR,
    MTOK_NUMBER,
    MTOK_EOF,
} map_token_kind_t;

typedef struct map_token_s
{
    int kind;

    float num_value;
    string_t string_value;

    const char *start, *end;
} map_token_t;

typedef struct map_parser_s
{
    const char *start, *at, *end;
    map_token_t token;
    map_token_t prev_token;
} map_parser_t;

// don't call map_begin_parse_ directly, use map_begin_parse, and handle the else case because it uses setjmp
static bool map_begin_parse_(map_parser_t *parser, string_t string);
static bool map_continue_parse(map_parser_t *parser);
static void map_parse_error(map_parser_t *parser, string_t error);
static bool map_next_token(map_parser_t *parser);
static void map_force_text_token(map_parser_t *parser);
static bool map_peek_token(map_parser_t *parser, map_token_kind_t kind);
static bool map_match_token(map_parser_t *parser, map_token_kind_t kind);
static bool map_require_token(map_parser_t *parser, map_token_kind_t kind);
static void map_parse_number(map_parser_t *parser, float *number);
static void map_parse_point(map_parser_t *parser, v3_t *result);
static void map_parse_tex_vec(map_parser_t *parser, v4_t *result);
static void map_parse_string(map_parser_t *parser, string_t *string);

#define map_begin_parse(parser, string) (map_begin_parse_(parser, string) && !setjmp(THREAD_CONTEXT->jmp))

bool map_begin_parse_(map_parser_t *parser, string_t string)
{
    zero_struct(parser);

    parser->start = string.data;
    parser->at    = string.data;
    parser->end   = string.data + string.count;

    bool result = map_next_token(parser);
    return result;
}

bool map_continue_parse(map_parser_t *parser)
{
    return parser->token.kind != MTOK_EOF;
}

void map_parse_error(map_parser_t *parser, string_t error)
{
    int line = 1, col = 0;

    for (const char *at = parser->start; at < parser->at; at++)
    {
        if (*at == '\n')
        {
            line += 1;
            col = 0;
        }
        else
        {
            col += 1;
        }
    }

    debug_print("parser error at line %d col %d: %.*s\n", line, col, strexpand(error));
    longjmp(THREAD_CONTEXT->jmp, 1);
}

static void map_skip_whitespace(map_parser_t *parser)
{
    const char *at  = parser->at;
    const char *end = parser->end;

    while (at < end)
    {
        switch (*at)
        {
            case ' ':
            case '\n':
            case '\r':
            case '\t':
            {
                at++;
            } break;

            case '/':
            {
                if (at + 1 < end && at[1] == '/')
                {
                    at += 2;
                    while (at < end && at[0] != '\n') at++;
                }
            } break;

            default:
            {
                goto done;
            } break;
        }
    }
done:

    parser->at = at;
}

bool map_next_token(map_parser_t *parser)
{
    map_skip_whitespace(parser);

    const char *at  = parser->at;
    const char *end = parser->end;

    parser->prev_token = parser->token;
    zero_struct(&parser->token);

    if (at >= end)
    {
        parser->token.kind = MTOK_EOF;
        return false;
    }

    const char *start = at;
    switch (at[0])
    {
        case '(':
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
        {
            parser->token.kind = *at++;
        } break;

        case '"':
        {
            at++;

            parser->token.kind = MTOK_STRING;
            parser->token.string_value.data = at;
            while (at < end && *at != '"')
            {
                at++;
            }
            parser->token.string_value.count = at - parser->token.string_value.data;
            if (*at == '"')
            {
                at++;
            }
            else
            {
                map_parse_error(parser, strlit("unterminated string"));
            }
        } break;

        case '-': case '+':
        case '0': case '1': case '2': case '3': case '4': 
        case '5': case '6': case '7': case '8': case '9':
        {
            parser->token.kind = MTOK_NUMBER;

            const char *strtod_end;
            parser->token.num_value = (float)strtod(at, &strtod_end);

            // texture names may (and do) start with symbols like +
            if (strtod_end == at)
            {
                goto parse_text;
            }

            at = strtod_end;
        } break;

        default:
        {
parse_text:
            parser->token.kind = MTOK_TEXT;

            at++;
            while (!is_whitespace(*at)) at++;

            parser->token.string_value = (string_t){
                .data  = start,
                .count = at - start,
            };
        } break;
    }

    parser->token.start = start;
    parser->token.end   = at;
    parser->at = at;

    return true;
}

void map_force_text_token(map_parser_t *parser)
{
    // texture names are not delineated with quotes or anything,
    // and can contain all kinds of symbols, so map_next_token
    // can't really handle them

    // this function will reinterpret the current token as an unquoted
    // string

    parser->at = parser->token.start;

    const char *at  = parser->at;
    const char *end = parser->end;

    const char *start = at;

    while (at < end && !is_whitespace(at[0]))
    {
        at++;
    }

    zero_struct(&parser->token);
    parser->token.kind  = MTOK_TEXT;
    parser->token.start = start;
    parser->token.end   = at;

    parser->token.string_value.data  = parser->token.start;
    parser->token.string_value.count = parser->token.end - parser->token.start;

    parser->at = at;
}

bool map_peek_token(map_parser_t *parser, map_token_kind_t kind)
{
    return parser->token.kind == kind;
}

bool map_match_token(map_parser_t *parser, map_token_kind_t kind)
{
    if (parser->token.kind == kind)
    {
        map_next_token(parser);
        return true;
    }
    else if (parser->token.kind == MTOK_EOF)
    {
        map_parse_error(parser, strlit("unexpected end of file"));
    }
    return false;
}

bool map_require_token(map_parser_t *parser, map_token_kind_t kind)
{
    if (parser->token.kind != kind)
    {
        map_parse_error(parser, strlit("required token"));
    }
    return map_next_token(parser);
}

void map_parse_number(map_parser_t *parser, float *number)
{
    if (!map_require_token(parser, MTOK_NUMBER)) 
        return;

    *number = parser->prev_token.num_value;
}

void map_parse_v3(map_parser_t *parser, v3_t *result)
{
    map_parse_number(parser, &result->x);
    map_parse_number(parser, &result->y);
    map_parse_number(parser, &result->z);
}

void map_parse_v4(map_parser_t *parser, v4_t *result)
{
    map_parse_number(parser, &result->x);
    map_parse_number(parser, &result->y);
    map_parse_number(parser, &result->z);
    map_parse_number(parser, &result->w);
}

void map_parse_point(map_parser_t *parser, v3_t *result)
{
    map_require_token(parser, '(');
    map_parse_v3(parser, result);
    map_require_token(parser, ')');
}

void map_parse_tex_vec(map_parser_t *parser, v4_t *result)
{
    map_require_token(parser, '[');
    map_parse_v4(parser, result);
    map_require_token(parser, ']');
}

void map_parse_string(map_parser_t *parser, string_t *string)
{
    if (!map_require_token(parser, MTOK_STRING))
        return;

    *string = parser->prev_token.string_value;
}

void map_parse_texture_name(map_parser_t *parser, string_t *string)
{
    if (!map_match_token(parser, MTOK_STRING))
        map_force_text_token(parser);

    *string = parser->token.string_value;

    map_next_token(parser);
}

map_entity_t *parse_map(arena_t *arena, string_t path)
{
    string_t map = fs_read_entire_file(temp, path);

    map_entity_t *f = NULL, *l = NULL;

    long debug_ordinal = 0;

    map_parser_t parser;
    if (map_begin_parse(&parser, map))
    {
        while (map_continue_parse(&parser))
        {
            // entity
            map_require_token(&parser, '{');

            map_entity_t *e = m_alloc_struct(arena, map_entity_t);
            sll_push_back(f, l, e);

            while (!map_match_token(&parser, '}'))
            {
                if (map_peek_token(&parser, MTOK_STRING))
                {
                    // property
                    string_t key = parser.token.string_value;
                    map_next_token(&parser);

                    string_t val;
                    map_parse_string(&parser, &val);

                    map_property_t *prop = m_alloc_struct(arena, map_property_t);
                    prop->key = string_copy(arena, key);
                    prop->val = string_copy(arena, val);
                    sll_push_back(e->first_property, e->last_property, prop);
                }
                else if (map_match_token(&parser, '{'))
                {
                    // brush
                    map_brush_t *brush = m_alloc_struct(arena, map_brush_t);
                    sll_push_back(e->first_brush, e->last_brush, brush);

                    while (!map_match_token(&parser, '}'))
                    {
                        map_plane_t *plane = m_alloc_struct(arena, map_plane_t);
                        plane->debug_ordinal = debug_ordinal++;

                        map_parse_point(&parser, &plane->a);
                        map_parse_point(&parser, &plane->b);
                        map_parse_point(&parser, &plane->c);

                        string_t texture;
                        map_parse_texture_name(&parser, &texture);

                        plane->texture = string_copy(arena, texture);

                        map_parse_tex_vec(&parser, &plane->s);
                        map_parse_tex_vec(&parser, &plane->t);

                        plane->lm_s = normalize_or_zero(plane->s.xyz);
                        plane->lm_t = normalize_or_zero(plane->t.xyz);

                        map_parse_number(&parser, &plane->rot);
                        map_parse_number(&parser, &plane->scale_x);
                        map_parse_number(&parser, &plane->scale_y);
                        
                        sll_push_back(brush->first_plane, brush->last_plane, plane);
                    }
                }
                else
                {
                    map_parse_error(&parser, strlit("unexpected token"));
                }
            }
        }
    }
    else
    {
        // map parse failed
        f = l = NULL;
    }

    return f;
}

//
// map geometry generation
//

typedef struct map_cached_texture_t
{
    STRING_STORAGE(256) name;

    unsigned w, h;
    resource_handle_t gpu_handle;
    image_t image;
} map_cached_texture_t;

static hash_t g_texture_cache_hash;
static bulk_t g_texture_cache = INIT_BULK_DATA(map_cached_texture_t);

static void generate_points_for_brush(arena_t *arena, map_brush_t *brush)
{
    ASSERT(arena != temp);

    arena_marker_t temp_marker = m_get_marker(temp);

    map_plane_t **planes = NULL;

    // TODO: Bit awkward. Should we be storing planes in a linked list? Is stretchy buffer the way for the map building stage?
    for (map_plane_t *plane = brush->first_plane; plane; plane = plane->next)
    {
        sb_push(planes, plane);
    }

    size_t plane_count = sb_count(planes);

    v3_t *vertices = NULL;

    unsigned  **plane_index_arrays = m_alloc_array(temp,  plane_count, unsigned *);
    map_poly_t *polys              = m_alloc_array(arena, plane_count, map_poly_t);

    brush->poly_count = plane_count;
    brush->polys      = polys;

    // intersect all 3-sized subsets of planes with each other to try and find some vertices

    for (subset_iter_t iter = iterate_subsets(temp, plane_count, 3); 
         subset_valid(&iter); 
         subset_next(&iter))
    {
        size_t plane0_index = iter.indices[0];
        size_t plane1_index = iter.indices[1];
        size_t plane2_index = iter.indices[2];

        map_plane_t *plane0 = planes[plane0_index];
        map_plane_t *plane1 = planes[plane1_index];
        map_plane_t *plane2 = planes[plane2_index];

        plane_t p0, p1, p2;
        plane_from_points(plane0->a, plane0->b, plane0->c, &p0);
        plane_from_points(plane1->a, plane1->b, plane1->c, &p1);
        plane_from_points(plane2->a, plane2->b, plane2->c, &p2);

        mat_t m = {
            .m = 4,
            .n = 3,
            .e = (float[4*3]){
                p0.n.x, p1.n.x, p2.n.x,
                p0.n.y, p1.n.y, p2.n.y,
                p0.n.z, p1.n.z, p2.n.z,
                p0.d,   p1.d,   p2.d,
            },
        };

        float x[3];
        if (solve_system_of_equations(&m, x))
        {
            unsigned vertex_index = sb_count(vertices);

            v3_t vertex = { x[0], x[1], x[2] };
            sb_push(vertices, vertex);

            sb_push(plane_index_arrays[plane0_index], vertex_index);
            sb_push(plane_index_arrays[plane1_index], vertex_index);
            sb_push(plane_index_arrays[plane2_index], vertex_index);
        }
    }

    // reject vertices that fall on the wrong side of any plane

    for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
    {
        map_plane_t *plane = planes[plane_index];

        plane_t p;
        plane_from_points(plane->a, plane->b, plane->c, &p);

        for (size_t vertex_index = 0; vertex_index < sb_count(vertices); vertex_index++)
        {
            v3_t v = vertices[vertex_index];
            float d = dot(p.n, v);

            if (d - 0.01f > p.d)
            {
                // mark for removal
                vertices[vertex_index] = (v3_t) { FLT_MAX, FLT_MAX, FLT_MAX };
            }
        }
    }

    // now remove those vertices for each plane

    for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
    {
        map_plane_t *plane = planes[plane_index];
        unsigned *plane_verts = plane_index_arrays[plane_index];

        for (size_t vert_index = 0; vert_index < sb_count(plane_verts);)
        {
            v3_t v = vertices[plane_verts[vert_index]];

            plane_t p;
            plane_from_points(plane->a, plane->b, plane->c, &p);

            if (v.x == FLT_MAX &&
                v.y == FLT_MAX &&
                v.z == FLT_MAX)
            {
                sb_remove_unordered(plane_verts, vert_index);
            }
            else
            {
                vert_index++;
            }
        }
    }

    // sort plane vertices to wind counter-clockwise

    // and while I'm at it compute the bounds
    rect3_t bounds = rect3_inverted_infinity();

    for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
    {
        map_plane_t *plane = planes[plane_index];
        unsigned    *indices = plane_index_arrays[plane_index];

        unsigned index_count = sb_count(indices);

        plane_t p;
        plane_from_points(plane->a, plane->b, plane->c, &p);

        v3_t t, b;
        get_tangent_vectors(p.n, &t, &b);

        v3_t v_mean = { 0, 0, 0 };
        for (size_t i = 0; i < index_count; i++)
        {
            v3_t v = vertices[indices[i]];
            v_mean = add(v_mean, v);

            bounds = rect3_grow_to_contain(bounds, v);
        }
        v_mean = div(v_mean, (float)index_count);

        m_scoped(temp)
        {
            float *angles = m_alloc_array_nozero(temp, index_count, float);

            for (size_t i = 0; i < index_count; i++)
            {
                v3_t v = sub(vertices[indices[i]], v_mean);

                float x = dot(t, v);
                float y = dot(b, v);

                float a = atan2f(y, x);

                if (a <= 0.0f) 
                    a += 2.0f*PI32;

                angles[i] = a;
            }

            for (size_t i = 0; i < index_count - 1; i++)
            {
                for (size_t j = i + 1; j < index_count; j++)
                {
                    if (angles[i] < angles[j])
                    {
                        SWAP(float,     angles [i],  angles [j]);
                        SWAP(unsigned,  indices[i],  indices[j]);
                    }
                }
            }
        }

        // remove vertices that are too close together

        for (size_t i = 0; i < index_count;)
        {
            v3_t v0 = vertices[indices[i]];
            v3_t v1 = vertices[indices[(i + 1) % index_count]];

            v3_t  d = sub(v1, v0);
            float l = vlensq(d);

            if (l < 0.01f)
            {
                sb_remove_ordered(indices, i);
                index_count -= 1;
            }
            else
            {
                i++;
            }
        }

        // find best fitting lightmap basis vectors

        // try and find a reasonable set of basis vectors for the lightmap
        // trying to minimize the surface area
        // as one of them

        float smallest_surface_area = FLT_MAX;
        v3_t  best_fit_lm_s      = { 0 };
        v3_t  best_fit_lm_t      = { 0 };
        v3_t  best_fit_lm_o      = { 0 };
        float best_fit_lm_w      = 0.0f;
        float best_fit_lm_h      = 0.0f;

        for (size_t i = 0; i < index_count - 1; i++)
        {
            v3_t v0 = vertices[indices[i + 0]];
            v3_t v1 = vertices[indices[i + 1]];

            v3_t lm_s = normalize(sub(v1, v0));
            v3_t lm_t = normalize(cross(p.n, lm_s));

            v2_t mins = {  FLT_MAX,  FLT_MAX };
            v2_t maxs = { -FLT_MAX, -FLT_MAX };

            for (size_t test_vertex_index = 0; test_vertex_index < index_count; test_vertex_index++)
            {
                v3_t test_vertex = vertices[indices[test_vertex_index]];

                v2_t st = { dot(test_vertex, lm_s), dot(test_vertex, lm_t) };
                mins = min(mins, st);
                maxs = max(maxs, st);
            }

            float w = maxs.x - mins.x;
            float h = maxs.y - mins.y;

            float surface_area = w*h;

            if (smallest_surface_area > surface_area)
            {
                v3_t o = mul(p.n, p.d);
                o = add(o, mul(lm_s, mins.x));
                o = add(o, mul(lm_t, mins.y));

                smallest_surface_area = surface_area;
                best_fit_lm_s = lm_s;
                best_fit_lm_t = lm_t;
                best_fit_lm_o = o;
                best_fit_lm_w = w;
                best_fit_lm_h = h;
            }
        }

        float scale_x = max(1.0f, LIGHTMAP_SCALE*ceilf(best_fit_lm_w / LIGHTMAP_SCALE));
        float scale_y = max(1.0f, LIGHTMAP_SCALE*ceilf(best_fit_lm_h / LIGHTMAP_SCALE));

        plane->lm_origin = best_fit_lm_o;
        plane->lm_s = best_fit_lm_s;
        plane->lm_t = best_fit_lm_t;
        plane->lm_scale_x = scale_x;
        plane->lm_scale_y = scale_y;
        plane->lm_tex_w = (int)(scale_x / LIGHTMAP_SCALE);
        plane->lm_tex_h = (int)(scale_y / LIGHTMAP_SCALE);
    }

    brush->bounds = bounds;

    // create poly (load texture and triangulate)

    for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
    {
        map_plane_t *plane = planes[plane_index];
        map_poly_t  *poly  = &polys[plane_index];

        plane->poly_index = (unsigned)plane_index;

        float texscale_x = MISSING_TEXTURE_SIZE;
        float texscale_y = MISSING_TEXTURE_SIZE;

        // see if texture was loaded before

        uint64_t value;
        if (hash_find(&g_texture_cache_hash, string_hash(plane->texture), &value))
        {
            resource_handle_t handle = { .value = value };

            map_cached_texture_t *cached = bd_get(&g_texture_cache, handle);
            ASSERT(string_match(plane->texture, STRING_FROM_STORAGE(cached->name)));

            texscale_x = (float)cached->w;
            texscale_y = (float)cached->h;
            poly->texture = cached->gpu_handle;
            poly->texture_cpu = cached->image;
        }

        // load texture if required

        if (!RESOURCE_HANDLE_VALID(poly->texture))
        {
            m_scoped(temp)
            {
                // TODO: pretty sad... handle file formats properly...
                string_t texture_path_png = string_format(temp, "gamedata/textures/%.*s.png", strexpand(plane->texture));
                string_t texture_path_tga = string_format(temp, "gamedata/textures/%.*s.tga", strexpand(plane->texture));

                image_t image = load_image(arena, texture_path_png, 4);

                if (!image.pixels)
                    image = load_image(arena, texture_path_tga, 4);

                if (image.pixels)
                {
                    poly->texture_cpu = image;

                    texscale_x = (float)image.w;
                    texscale_y = (float)image.h;
                    poly->texture = render->upload_texture(&(upload_texture_t) {
                        .desc = {
                            .format = PIXEL_FORMAT_SRGB8_A8,
                            .w      = image.w,
                            .h      = image.h,
                            .pitch  = image.pitch,
                        },
                        .data = {
                            .pixels = image.pixels,
                        },
                    });

                    map_cached_texture_t *cached = bd_add(&g_texture_cache);
                    cached->w          = image.w;
                    cached->h          = image.h;
                    cached->gpu_handle = poly->texture;
                    cached->image      = poly->texture_cpu;
                    STRING_INTO_STORAGE(cached->name, plane->texture);

                    resource_handle_t handle = bd_get_handle(&g_texture_cache, cached);
                    hash_add(&g_texture_cache_hash, string_hash(plane->texture), handle.value);
                }
            }
        }

        // triangulate

        m_scoped(temp)
        {
            unsigned *indices = plane_index_arrays[plane_index];

            if (sb_count(indices) >= 3)
            {
                plane_t p;
                plane_from_points(plane->a, plane->b, plane->c, &p);
                
                poly->normal = p.n;

                v3_t  s_vec    = { plane->s.x, plane->s.y, plane->s.z };
                float s_offset = plane->s.w;

                v3_t  t_vec    = { plane->t.x, plane->t.y, plane->t.z };
                float t_offset = plane->t.w;

                unsigned vertex_count = sb_count(indices);
                vertex_brush_t *triangle_vertices = m_alloc_array_nozero(arena, vertex_count, vertex_brush_t);

                for (size_t i = 0; i < vertex_count; i++)
                {
                    v3_t pos = vertices[indices[i]];
                    triangle_vertices[i] = (vertex_brush_t) {
                        .pos = pos,
                        .tex = {
                            .x = (dot(pos, s_vec) + s_offset) / texscale_x / plane->scale_x,
                            .y = (dot(pos, t_vec) + t_offset) / texscale_y / plane->scale_y,
                        },
                        .tex_lightmap = {
                            .x = dot(sub(pos, plane->lm_origin), plane->lm_s) / plane->lm_scale_x,
                            .y = dot(sub(pos, plane->lm_origin), plane->lm_t) / plane->lm_scale_y,
                        },
                    };
                }

                unsigned triangle_count = vertex_count - 2;
                unsigned index_count    = 3*triangle_count;
                uint16_t *triangle_indices = m_alloc_array_nozero(arena, index_count, uint16_t);

                unsigned triangle_offset = 0;
                for (uint16_t i = 1; i < vertex_count - 1; i++)
                {
                    triangle_indices[triangle_offset++] = 0;
                    triangle_indices[triangle_offset++] = i;
                    triangle_indices[triangle_offset++] = i + 1;
                }

                poly->index_count  = index_count;
                poly->vertex_count = vertex_count;
                poly->indices      = triangle_indices;
                poly->vertices     = triangle_vertices;

                poly->mesh = render->upload_model(&(upload_model_t) {
                    .vertex_format = VERTEX_FORMAT_BRUSH,
                    .vertex_count  = poly->vertex_count,
                    .vertices      = poly->vertices,
                    .index_count   = poly->index_count,
                    .indices       = poly->indices,
                });
            }
        }
    }

    m_reset_to_marker(temp, temp_marker);
}

typedef struct partition_result_t
{
    uint32_t split_index;
} partition_result_t;

static void partition_brushes(map_t *map, uint32_t first, uint32_t count, rect3_t bv, uint8_t split_axis, partition_result_t *result)
{
    map_brush_t **brushes = map->brushes + first;

    float pivot = 0.5f*(bv.min.e[split_axis] + bv.max.e[split_axis]);

    int64_t i = -1;
    int64_t j = (int64_t)count;

    for (;;)
    {
        for (;;)
        {
            i += 1;

            if (i >= count)
                break;

            float p = 0.5f*(brushes[i]->bounds.min.e[split_axis] + 
                            brushes[i]->bounds.max.e[split_axis]);

            if (p > pivot)
                break;
        }

        for (;;)
        {
            j -= 1;

            if (j < 0)
                break;

            float p = 0.5f*(brushes[j]->bounds.min.e[split_axis] + 
                            brushes[j]->bounds.max.e[split_axis]);

            if (p < pivot)
                break;
        }

        if (i >= j)
            break;

        SWAP(map_brush_t *, brushes[i], brushes[j]);
    }

    result->split_index = (uint32_t)i;

#ifdef DEBUG_SLOW
    for (size_t brush_index = 0; brush_index < count; brush_index++)
    {
        float p = 0.5f*(brushes[brush_index]->bounds.min.e[split_axis] + 
                        brushes[brush_index]->bounds.max.e[split_axis]);
        if (brush_index < result->split_index)
        {
            ASSERT(p <= pivot);
        }
        else
        {
            ASSERT(p >= pivot);
        }
    }
#endif

    // awkward, debatable whether it's good to do this
    if (result->split_index == count)
        result->split_index -= 1;
}

static rect3_t compute_bounding_volume(map_t *map, uint32_t first, uint32_t count)
{
    rect3_t bv = rect3_inverted_infinity();

    map_brush_t **brushes = map->brushes + first;
    for (size_t i = 0; i < count; i++)
    {
        map_brush_t *brush = brushes[i];
        bv = rect3_union(bv, brush->bounds);
    }

    return bv;
}

static void build_bvh_recursively(map_t *map, map_bvh_node_t *parent, uint32_t first, uint32_t count)
{
    rect3_t bv = compute_bounding_volume(map, first, count);
    parent->bounds = bv;

    bool leaf = (count == 1);

    partition_result_t partition = {0};
    uint8_t split_axis = rect3_largest_axis(bv);

    if (!leaf)
    {
        if (count == 2)
        {
            partition.split_index = 1;
        }
        else
        {
            leaf = true;
            for (size_t i = 0; i < 3; i++)
            {
                partition_brushes(map, first, count, bv, split_axis, &partition);

                uint32_t max_index = count - 1;

                if (partition.split_index == 0 ||
                    partition.split_index == max_index)
                {
                    split_axis = (split_axis + 1) % 3;
                }
                else
                {
                    leaf = false;
                    break;
                }
            }
        }
    }

    if (leaf)
    {        
        // TOOD: Write some trapped casts already
        if (NEVER(count > UINT16_MAX))
            count = UINT16_MAX;

        parent->left_first = first;
        parent->count      = (uint16_t)count;
    }
    else
    {
        parent->split_axis = split_axis;

        uint32_t l_node_index = map->node_count++;
        uint32_t r_node_index = map->node_count++;

        parent->left_first = l_node_index;

        map_bvh_node_t *l = &map->nodes[l_node_index];
        uint32_t l_split_index = first;
        uint32_t l_split_count = partition.split_index;
        build_bvh_recursively(map, l, l_split_index, l_split_count);

        map_bvh_node_t *r = &map->nodes[r_node_index];
        uint32_t r_split_index = first + partition.split_index;
        uint32_t r_split_count = count - partition.split_index;
        build_bvh_recursively(map, r, r_split_index, r_split_count);
    }

}

static void build_bvh(arena_t *arena, map_t *map)
{
    size_t max_nodes_count = 2ull*map->brush_count;

    if (NEVER(max_nodes_count > UINT32_MAX)) 
        max_nodes_count = UINT32_MAX;

    map->nodes = m_alloc_nozero(arena, max_nodes_count*sizeof(map_bvh_node_t), 64); 

    map_bvh_node_t *root = &map->nodes[map->node_count++];
    map->node_count++; // leave a gap after the root to make pairs of nodes end up on the same cache line

    build_bvh_recursively(map, root, 0, map->brush_count);
}

static void gather_lights(arena_t *arena, map_t *map)
{
    map_point_light_t *lights = NULL;

    for (map_entity_t *entity = map->first_entity; entity; entity = entity->next)
    {
        if (is_class(entity, strlit("point_light")))
        {
            float brightness = float_from_key(entity, strlit("brightness"));
            v3_t  color      = v3_from_key(entity, strlit("_color"));

            map_point_light_t light = {
                .p     = v3_from_key(entity, strlit("origin")),
                .color = mul(brightness, color),
            };
            sb_push(lights, light);
        }
    }

    map->light_count = sb_count(lights);
    map->lights = sb_copy(arena, lights);
}

map_t *load_map(arena_t *arena, string_t path)
{
    map_t *map = m_alloc_struct(arena, map_t);
    map->first_entity = parse_map(arena, path);

    uint32_t brush_count = 0;

    for (map_entity_t *entity = map->first_entity; entity; entity = entity->next)
    {
        for (map_brush_t *brush = entity->first_brush; brush; brush = brush->next)
        {
            generate_points_for_brush(arena, brush);
            brush_count++;
        }
    }

    map->brush_count = brush_count;

    // TODO: bad code, bad man.

    map->brushes = m_alloc_array_nozero(arena, brush_count, map_brush_t *);

    size_t brush_index = 0;

    for (map_entity_t *entity = map->first_entity; entity; entity = entity->next)
    {
        for (map_brush_t *brush = entity->first_brush; brush; brush = brush->next)
        {
            map->brushes[brush_index++] = brush;
        }
    }

    build_bvh(arena, map);
    map->bounds = map->nodes[0].bounds;

    gather_lights(arena, map);

    return map;
}

bool is_class(map_entity_t *entity, string_t classname)
{
    return string_match(value_from_key(entity, strlit("classname")), classname);
}

string_t value_from_key(map_entity_t *entity, string_t key)
{
    string_t result = { 0 };

    for (map_property_t *prop = entity->first_property; prop; prop = prop->next)
    {
        if (string_match(key, prop->key))
        {
            result = prop->val;
            break;
        }
    }

    return result;
}

int int_from_key(map_entity_t *entity, string_t key)
{
    string_t value = value_from_key(entity, key);

    int64_t result = 0;
    string_parse_int(&value, &result);

    return (int)result;
}

float float_from_key(map_entity_t *entity, string_t key)
{
    string_t value = value_from_key(entity, key);

    float result = 0.0f;
    string_parse_float(&value, &result);

    return result;
}

v3_t v3_from_key(map_entity_t *entity, string_t key)
{
    string_t value = value_from_key(entity, key);

    v3_t v3 = { 0 };

    string_parse_float(&value, &v3.x); 
    string_parse_float(&value, &v3.y); 
    string_parse_float(&value, &v3.z); 

    return v3;
}
