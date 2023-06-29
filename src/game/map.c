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

typedef struct map_entity_node_t
{
    struct map_entity_node_t *next;
    map_entity_t entity;
} map_entity_node_t;

typedef struct map_property_node_t
{
    struct map_property_node_t *next;
    map_property_t property;
} map_property_node_t;

typedef struct map_brush_node_t
{
    struct map_brush_node_t *next;
    map_brush_t brush;
} map_brush_node_t;

typedef struct map_plane_node_t
{
    struct map_plane_node_t *next;
    map_plane_t plane;
} map_plane_node_t;

typedef struct map_parse_result_t
{
    uint32_t entity_count;
    uint32_t property_count;
    uint32_t brush_count;
    uint32_t plane_count;

    map_entity_t   *entities;
    map_property_t *properties;
    map_brush_t    *brushes;
    map_plane_t    *planes;
} map_parse_result_t;

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

static bool parse_map(arena_t *arena, string_t path, map_parse_result_t *result)
{
    zero_struct(result);

    string_t map_file = fs_read_entire_file(temp, path);

    map_entity_node_t   *first_entity   = NULL, *last_entity   = NULL;
    map_property_node_t *first_property = NULL, *last_property = NULL;
    map_brush_node_t    *first_brush    = NULL, *last_brush    = NULL;
    map_plane_node_t    *first_plane    = NULL, *last_plane    = NULL;

    bool parsed_successfully = false;

    map_parser_t parser;
    if (map_begin_parse(&parser, map_file))
    {
        while (map_continue_parse(&parser))
        {
            // entity
            map_require_token(&parser, '{');

            map_entity_node_t *e_node = m_alloc_struct(temp, map_entity_node_t);
            map_entity_t *e = &e_node->entity;

            sll_push_back(first_entity, last_entity, e_node);
            result->entity_count += 1;

            e->first_brush_edge = result->brush_count;
            e->first_property   = result->property_count;

            while (!map_match_token(&parser, '}'))
            {
                if (map_peek_token(&parser, MTOK_STRING))
                {
                    // property
                    string_t key = parser.token.string_value;
                    map_next_token(&parser);

                    string_t val;
                    map_parse_string(&parser, &val);

                    map_property_node_t *prop_node = m_alloc_struct(temp, map_property_node_t);
                    map_property_t *prop = &prop_node->property;

                    sll_push_back(first_property, last_property, prop_node);
                    result->property_count += 1;

                    prop->key = string_copy(arena, key);
                    prop->val = string_copy(arena, val);
                }
                else if (map_match_token(&parser, '{'))
                {
                    // brush

                    map_brush_node_t *brush_node = m_alloc_struct(temp, map_brush_node_t);
                    map_brush_t *brush = &brush_node->brush;

                    brush->first_plane_poly = result->plane_count;

                    sll_push_back(first_brush, last_brush, brush_node);
                    result->brush_count += 1;

                    while (!map_match_token(&parser, '}'))
                    {
                        map_plane_node_t *plane_node = m_alloc_struct(temp, map_plane_node_t);
                        map_plane_t *plane = &plane_node->plane;

                        sll_push_back(first_plane, last_plane, plane_node);
                        result->plane_count += 1;

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
                    }

                    brush->plane_poly_count = result->plane_count - brush->first_plane_poly;

                    if (brush->plane_poly_count == 0) brush->first_plane_poly = 0;
                }
                else
                {
                    map_parse_error(&parser, strlit("unexpected token"));
                }
            }

            e->brush_count    = result->brush_count    - e->first_brush_edge;
            e->property_count = result->property_count - e->first_property;

            if (e->brush_count    == 0) e->first_brush_edge = 0;
            if (e->property_count == 0) e->first_property   = 0;
        }

        //
        // Linearize Result
        //

        size_t i;

        // entities

        result->entities = m_alloc_array_nozero(arena, result->entity_count, map_entity_t);

        i = 0;
        for (map_entity_node_t *entity_node = first_entity; entity_node; entity_node = entity_node->next)
        {
            copy_struct(&result->entities[i++], &entity_node->entity);
        }

        // properties

        result->properties = m_alloc_array_nozero(arena, result->property_count, map_property_t);

        i = 0;
        for (map_property_node_t *property_node = first_property; property_node; property_node = property_node->next)
        {
            copy_struct(&result->properties[i++], &property_node->property);
        }

        // brushes

        result->brushes = m_alloc_array_nozero(arena, result->brush_count, map_brush_t);

        i = 0;
        for (map_brush_node_t *brush_node = first_brush; brush_node; brush_node = brush_node->next)
        {
            copy_struct(&result->brushes[i++], &brush_node->brush);
        }

        // planes

        result->planes = m_alloc_array_nozero(arena, result->plane_count, map_plane_t);

        i = 0;
        for (map_plane_node_t *plane_node = first_plane; plane_node; plane_node = plane_node->next)
        {
            copy_struct(&result->planes[i++], &plane_node->plane);
        }

        //

        parsed_successfully = true;
    }
    else
    {
        // map parse failed
        result->entity_count   = 0;
        result->property_count = 0;
        result->brush_count    = 0;
        result->plane_count    = 0;
    }

    return parsed_successfully;
}

//
// map geometry generation
//

static void generate_map_geometry(arena_t *arena, map_t *map)
{
    ASSERT(arena != temp);

    arena_marker_t temp_marker = m_get_marker(temp);

    // these are for building up the final map geometry
    // TODO: do some reservation to avoid pointless copying
    stretchy_buffer(uint16_t) map_indices            = NULL;
    stretchy_buffer(v3_t)     map_positions          = NULL;
    stretchy_buffer(v2_t)     map_texcoords          = NULL;
    stretchy_buffer(v2_t)     map_lightmap_texcoords = NULL;

	map->poly_count = map->plane_count;
	map->polys      = m_alloc_array(arena, map->poly_count, map_poly_t);

    for (size_t brush_index = 0; brush_index < map->brush_count; brush_index++)
    {
        map_brush_t *brush = &map->brushes[brush_index];

        size_t       plane_count = brush->plane_poly_count;
        map_plane_t *planes      = map->planes + brush->first_plane_poly;
        map_poly_t  *polys       = map->polys  + brush->first_plane_poly;

        stretchy_buffer(v3_t)      brush_positions     = NULL;
        stretchy_buffer(uint16_t) *plane_index_buffers = m_alloc_array(temp, plane_count, stretchy_buffer(uint16_t));

        // intersect all 3-sized subsets of planes with each other to try and find some vertices

        for (subset_iter_t iter = iterate_subsets(temp, plane_count, 3); 
             subset_valid(&iter); 
             subset_next(&iter))
        {
            size_t plane0_index = iter.indices[0];
            size_t plane1_index = iter.indices[1];
            size_t plane2_index = iter.indices[2];

            map_plane_t *plane0 = &planes[plane0_index];
            map_plane_t *plane1 = &planes[plane1_index];
            map_plane_t *plane2 = &planes[plane2_index];

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
                uint16_t index = (uint16_t)sb_count(brush_positions);

                v3_t position = { x[0], x[1], x[2] };
                sb_push(brush_positions, position);

                sb_push(plane_index_buffers[plane0_index], index);
                sb_push(plane_index_buffers[plane1_index], index);
                sb_push(plane_index_buffers[plane2_index], index);
            }
        }

        // reject vertices that fall on the wrong side of any plane

        for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
        {
            map_plane_t *plane = &planes[plane_index];

            plane_t p;
            plane_from_points(plane->a, plane->b, plane->c, &p);

            for (size_t vertex_index = 0; vertex_index < sb_count(brush_positions); vertex_index++)
            {
                v3_t v = brush_positions[vertex_index];
                float d = dot(p.n, v);

                if (d - 0.01f > p.d)
                {
                    // mark for removal
                    brush_positions[vertex_index] = make_v3(FLT_MAX, FLT_MAX, FLT_MAX);
                }
            }
        }

        // now remove those vertices for each plane

        for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
        {
            map_plane_t              *plane         = &planes[plane_index];
            stretchy_buffer(uint16_t) plane_indices = plane_index_buffers[plane_index];

            for (size_t index_index = 0; index_index < sb_count(plane_indices);)
            {
                v3_t v = brush_positions[plane_indices[index_index]];

                plane_t p;
                plane_from_points(plane->a, plane->b, plane->c, &p);

                if (v.x == FLT_MAX &&
                    v.y == FLT_MAX &&
                    v.z == FLT_MAX)
                {
                    sb_remove_unordered(plane_indices, index_index);
                }
                else
                {
                    index_index++;
                }
            }
        }

        // sort plane vertices to wind counter-clockwise

        // and while I'm at it compute the bounds
        rect3_t bounds = rect3_inverted_infinity();

        for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
        {
            map_plane_t              *plane         = &planes[plane_index];
            stretchy_buffer(uint16_t) plane_indices = plane_index_buffers[plane_index];

            uint32_t index_count = sb_count(plane_indices);

            plane_t p;
            plane_from_points(plane->a, plane->b, plane->c, &p);

            v3_t t, b;
            get_tangent_vectors(p.n, &t, &b);

            v3_t v_mean = { 0, 0, 0 };
            for (size_t i = 0; i < index_count; i++)
            {
                v3_t v = brush_positions[plane_indices[i]];
                v_mean = add(v_mean, v);

                bounds = rect3_grow_to_contain(bounds, v);
            }
            v_mean = div(v_mean, (float)index_count);

            m_scoped(temp)
            {
                float *angles = m_alloc_array_nozero(temp, index_count, float);

                for (size_t i = 0; i < index_count; i++)
                {
                    v3_t v = sub(brush_positions[plane_indices[i]], v_mean);

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
                            SWAP(float,     angles       [i],  angles       [j]);
                            SWAP(uint16_t,  plane_indices[i],  plane_indices[j]);
                        }
                    }
                }
            }

            // remove vertices that are too close together

            for (size_t i = 0; i < index_count;)
            {
                v3_t v0 = brush_positions[plane_indices[i]];
                v3_t v1 = brush_positions[plane_indices[(i + 1) % index_count]];

                v3_t  d = sub(v1, v0);
                float l = vlensq(d);

                if (l < 0.01f)
                {
                    sb_remove_ordered(plane_indices, i);
                    index_count -= 1;
                }
                else
                {
                    i++;
                }
            }

            // find least wasteful lightmap orientation

            float smallest_surface_area = FLT_MAX;
            v3_t  best_fit_lm_s      = { 0 };
            v3_t  best_fit_lm_t      = { 0 };
            v3_t  best_fit_lm_o      = { 0 };
            float best_fit_lm_w      = 0.0f;
            float best_fit_lm_h      = 0.0f;

            for (size_t i = 0; i < index_count - 1; i++)
            {
                v3_t v0 = brush_positions[plane_indices[i + 0]];
                v3_t v1 = brush_positions[plane_indices[i + 1]];

                v3_t lm_s = normalize(sub(v1, v0));
                v3_t lm_t = normalize(cross(p.n, lm_s));

                v2_t mins = {  FLT_MAX,  FLT_MAX };
                v2_t maxs = { -FLT_MAX, -FLT_MAX };

                for (size_t test_vertex_index = 0; test_vertex_index < index_count; test_vertex_index++)
                {
                    v3_t test_vertex = brush_positions[plane_indices[test_vertex_index]];

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

            plane->lm_origin  = best_fit_lm_o;
            plane->lm_s       = best_fit_lm_s;
            plane->lm_t       = best_fit_lm_t;
            plane->lm_scale_x = scale_x;
            plane->lm_scale_y = scale_y;
            plane->lm_tex_w   = (int)(scale_x / LIGHTMAP_SCALE);
            plane->lm_tex_h   = (int)(scale_y / LIGHTMAP_SCALE);
        }

        brush->bounds = bounds;

        // create poly (load texture and triangulate)

        for (size_t plane_index = 0; plane_index < plane_count; plane_index++)
        {
            map_plane_t *plane = &planes[plane_index];
            map_poly_t  *poly  = &polys [plane_index];

            float texscale_x = MISSING_TEXTURE_SIZE;
            float texscale_y = MISSING_TEXTURE_SIZE;

            // load texture

			m_scoped(temp)
			{
				// TODO: pretty sad... handle file formats properly...
				asset_hash_t texture_png = asset_hash_from_string(string_format(temp, "gamedata/textures/%.*s.png", strexpand(plane->texture)));
				asset_hash_t texture_tga = asset_hash_from_string(string_format(temp, "gamedata/textures/%.*s.tga", strexpand(plane->texture)));
				// TODO ALSO: String + formatting helper function for asset hashes

				image_t *image = &missing_image;

				if (asset_exists(texture_png, ASSET_KIND_IMAGE))
				{
					image = get_image(texture_png);
				}
				else if (asset_exists(texture_tga, ASSET_KIND_IMAGE))
				{
					image = get_image(texture_tga);
				}

				poly->image = image;
				texscale_x = (float)image->w;
				texscale_y = (float)image->h;
				poly->texture = image->gpu; // TODO: Weird? Maybe?
            }

            // triangulate

            // NOTE: I got confused about this before. The plane indices used here are
            // used to reuse vertex positions for the brush. However, because each plane
            // has potentially a different texture and lightmap texture, planes can't
            // share vertices. That's why this code is the way it is. Daniël 02/12/2022

            // NOTE: Don't wrap this in a scoped temp block. This is one of those cases
            // where the Ryan-Allen style of having multiple scratch arenas to deal with
            // conflicts would help. Pushing to the map_* stretchy buffers means we can't
            // be resetting temp storage here.

            stretchy_buffer(uint16_t) plane_indices = plane_index_buffers[plane_index];

            if (sb_count(plane_indices) >= 3)
            {
                plane_t p;
                plane_from_points(plane->a, plane->b, plane->c, &p);
                
                poly->normal = p.n;

                v3_t  s_vec    = { plane->s.x, plane->s.y, plane->s.z };
                float s_offset = plane->s.w;

                v3_t  t_vec    = { plane->t.x, plane->t.y, plane->t.z };
                float t_offset = plane->t.w;

                uint32_t triangulated_vertex_count = sb_count(plane_indices); // See note above, the index count is the vertex count (for the triangulated geometry).
                vertex_brush_t *triangulated_vertices = m_alloc_array_nozero(arena, triangulated_vertex_count, vertex_brush_t);

                for (size_t vertex_index = 0; vertex_index < triangulated_vertex_count; vertex_index++)
                {
                    v3_t pos = brush_positions[plane_indices[vertex_index]];

                    triangulated_vertices[vertex_index] = (vertex_brush_t) {
                        .pos = pos,
                        .tex = {
                            .x = (dot(pos, s_vec) + s_offset) / texscale_x / plane->scale_x,
                            .y = (dot(pos, t_vec) + t_offset) / texscale_y / plane->scale_y,
                        },
                        .tex_lightmap = {
                            .x = dot(sub(pos, plane->lm_origin), plane->lm_s) / plane->lm_scale_x,
                            .y = dot(sub(pos, plane->lm_origin), plane->lm_t) / plane->lm_scale_y,
                        },
                        .normal = p.n,
                    };
                }

                uint32_t triangle_count = triangulated_vertex_count - 2;

                uint32_t triangulated_index_count = 3*triangle_count;
                uint16_t *triangulated_indices = m_alloc_array_nozero(arena, triangulated_index_count, uint16_t);

                size_t triangle_offset = 0;
                for (uint16_t i = 1; i < triangulated_vertex_count - 1; i++)
                {
                    triangulated_indices[triangle_offset++] = 0;
                    triangulated_indices[triangle_offset++] = i;
                    triangulated_indices[triangle_offset++] = i + 1;
                }

                poly->first_index  = sb_count(map_indices);
                poly->first_vertex = sb_count(map_positions);

                // TODO: Bit silly?

                for (size_t i = 0; i < triangulated_index_count; i++)
                {
                    sb_push(map_indices, triangulated_indices[i]);
                }

                for (size_t i = 0; i < triangulated_vertex_count; i++)
                {
                    sb_push(map_positions, triangulated_vertices[i].pos);
                    sb_push(map_texcoords, triangulated_vertices[i].tex);
                    sb_push(map_lightmap_texcoords, triangulated_vertices[i].tex_lightmap);
                }

                poly->index_count  = triangulated_index_count;
                poly->vertex_count = triangulated_vertex_count;

                poly->mesh = render->upload_model(&(upload_model_t) {
                    .vertex_format = VERTEX_FORMAT_BRUSH,
                    .index_count   = triangulated_index_count,
                    .indices       = triangulated_indices,
                    .vertex_count  = triangulated_vertex_count,
                    .vertices      = triangulated_vertices,
                });
            }
        }
    }

    map->indices = sb_copy(arena, map_indices);
    map->vertex.positions = sb_copy(arena, map_positions);
    map->vertex.texcoords = sb_copy(arena, map_texcoords);
    map->vertex.lightmap_texcoords = sb_copy(arena, map_lightmap_texcoords);

    m_reset_to_marker(temp, temp_marker);
}

typedef struct bvh_builder_context_t
{
    map_t *map;
    uint32_t *brush_indices;
} bvh_builder_context_t;

typedef struct partition_result_t
{
    uint32_t split_index;
} partition_result_t;

static void partition_brushes(bvh_builder_context_t *context, uint32_t first, uint32_t count, rect3_t bv, uint8_t split_axis, partition_result_t *result)
{
    map_t *map = context->map;

    uint32_t    *brush_edges = map->brush_edges + first;
    map_brush_t *brushes     = map->brushes;

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

            float p = 0.5f*(brushes[brush_edges[i]].bounds.min.e[split_axis] + 
                            brushes[brush_edges[i]].bounds.max.e[split_axis]);

            if (p > pivot)
                break;
        }

        for (;;)
        {
            j -= 1;

            if (j < 0)
                break;

            float p = 0.5f*(brushes[brush_edges[j]].bounds.min.e[split_axis] + 
                            brushes[brush_edges[j]].bounds.max.e[split_axis]);

            if (p < pivot)
                break;
        }

        if (i >= j)
            break;

        SWAP(uint32_t, brush_edges[i], brush_edges[j]);
    }

    result->split_index = (uint32_t)i;

#ifdef DEBUG_SLOW
    for (size_t index = 0; index < count; index++)
    {
        float p = 0.5f*(brushes[brush_edges[index]].bounds.min.e[split_axis] + 
                        brushes[brush_edges[index]].bounds.max.e[split_axis]);
        if (index < result->split_index)
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

static rect3_t compute_bounding_volume(bvh_builder_context_t *context, uint32_t first, uint32_t count)
{
    map_t *map = context->map;

    uint32_t    *brush_edges   = map->brush_edges + first;
    map_brush_t *brushes       = map->brushes;

    rect3_t bv = rect3_inverted_infinity();

    for (size_t i = 0; i < count; i++)
    {
        map_brush_t *brush = &brushes[brush_edges[i]];
        bv = rect3_union(bv, brush->bounds);
    }

    return bv;
}

static void build_bvh_recursively(bvh_builder_context_t *context, map_bvh_node_t *parent, uint32_t first, uint32_t count)
{
    map_t *map = context->map;

    rect3_t bv = compute_bounding_volume(context, first, count);
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
                partition_brushes(context, first, count, bv, split_axis, &partition);

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
        build_bvh_recursively(context, l, l_split_index, l_split_count);

        map_bvh_node_t *r = &map->nodes[r_node_index];
        uint32_t r_split_index = first + partition.split_index;
        uint32_t r_split_count = count - partition.split_index;
        build_bvh_recursively(context, r, r_split_index, r_split_count);
    }
}

static void build_bvh(arena_t *arena, map_t *map)
{
    size_t max_nodes_count = 2ull*map->brush_count;

    if (NEVER(max_nodes_count > UINT32_MAX)) 
        max_nodes_count = UINT32_MAX;

    bvh_builder_context_t context = {
        .map = map,
    };

    map->brush_edges = m_alloc_array_nozero(arena, map->brush_count, uint32_t);
    for (size_t i = 0; i < map->brush_count; i++)
    {
        map->brush_edges[i] = (uint32_t)i;
    }

    map->nodes = m_alloc_nozero(arena, max_nodes_count*sizeof(map_bvh_node_t), 64); 

    map_bvh_node_t *root = &map->nodes[map->node_count++];
    map->node_count++; // leave a gap after the root to make pairs of nodes end up on the same cache line

    build_bvh_recursively(&context, root, 0, map->brush_count);

    m_scoped(temp)
    {
        // NOTE: I am going to sort the brushes in memory according to the BVH, which means I have to
        // re-order the brush edges so that entities can use them to find brushes, because from the
        // point of view of the entities I am making a real mess of the brushes.

        // TODO: scratch_brush_edges is slightly roundabout, as I could've just passed the bvh builder
        // context a temporary brush_edges array (as I nearly did actually do) and avoided a copy.

        uint32_t    *scratch_brush_edges = m_copy_array(temp, map->brush_edges, map->brush_count);
        map_brush_t *scratch_brushes     = m_copy_array(temp, map->brushes,     map->brush_count);

        for (size_t index = 0; index < map->brush_count; index++)
        {
            uint32_t edge = scratch_brush_edges[index];

            // Invert the brush edge mapping
            map->brush_edges[edge] = (uint32_t)index;
            map->brushes[index] = scratch_brushes[edge];
        }
    }
}

static void gather_lights(arena_t *arena, map_t *map)
{
    map_point_light_t *lights = NULL;

    for (size_t entity_index = 0; entity_index < map->entity_count; entity_index++)
    {
        map_entity_t *entity = &map->entities[entity_index];

        if (is_class(map, entity, strlit("point_light")))
        {
            float brightness = float_from_key(map, entity, strlit("brightness"));
            v3_t  color      = v3_from_key(map, entity, strlit("_color"));

            map_point_light_t light = {
                .p     = v3_from_key(map, entity, strlit("origin")),
                .color = mul(brightness, color),
            };
            sb_push(lights, light);
        }
    }

    map->light_count = sb_count(lights);
    map->lights = sb_copy(arena, lights);
}

static void merge_coplanar_lightmaps(arena_t *arena, map_t *map)
{
	(void)arena;
	(void)map;
#if 0
	for (size_t poly_index = 0; poly_index < map->poly_count; poly_index++)
	{
		map_poly_t *poly = &map->polys[poly_index];

		for (size_t test_poly_index = 0; test_poly_index < map->poly_count; test_poly_index++)
		{
			if (test_poly_index == poly_index)
			{
				continue;
			}

			map_poly_t *test_poly = &map->polys[test_poly_index];

			if (!v3_equal(poly->normal, test_poly->normal, 0.01f))
			{
				continue;
			}

			if (!flt_equal(poly->distance, test_poly->distance, 0.01f))
			{
				continue;
			}

			// the polys need to share at least one edge.

			bool coplanar = false;

			for (size_t edge_index = 0; edge_index < poly->index_count - 1; edge_index++)
			{
				uint32_t edge_a = map->indices[poly->first_index + edge_index];
				uint32_t edge_b = map->indices[poly->first_index + edge_index + 1];

				v3_t vertex_a = map->vertices[edge_a];
				v3_t vertex_b = map->vertices[edge_b];

				for (size_t test_edge_index = 0; test_edge_index < test_poly->index_count - 1; test_edge_index)
				{
					uint32_t test_edge_a = map->indices[test_poly->first_index + test_edge_index];
					uint32_t test_edge_b = map->indices[test_poly->first_index + test_edge_index + 1];

					v3_t test_vertex_a = map->vertices[test_edge_a];
					v3_t test_vertex_b = map->vertices[test_edge_b];

					if ((v3_equal(test_vertex_a, vertex_a, 0.01f) && v3_equal(test_vertex_b, vertex_b, 0.01f)) ||
						(v3_equal(test_vertex_b, vertex_a, 0.01f) && v3_equal(test_vertex_a, vertex_b, 0.01f)))
					{
						coplanar = true;
						break;
					}
				}
			}

			if (coplanar)
			{
				// merge.
			}
		}
	}
#endif
}

map_t *load_map(arena_t *arena, string_t path)
{
    map_t *map = NULL;

    map_parse_result_t parse_result;
    bool successful_parse = parse_map(arena, path, &parse_result);

    if (successful_parse)
    {
        map = m_alloc_struct(arena, map_t);

        map->entity_count   = parse_result.entity_count;
        map->property_count = parse_result.property_count;
        map->brush_count    = parse_result.brush_count;
        map->plane_count    = parse_result.plane_count;

        map->entities   = parse_result.entities;
        map->properties = parse_result.properties;
        map->brushes    = parse_result.brushes;
        map->planes     = parse_result.planes;

        generate_map_geometry(arena, map);

		bool fix_lightmap_seams = false;
		if (fix_lightmap_seams)
		{
			merge_coplanar_lightmaps(arena, map);
		}

        build_bvh(arena, map);
        map->bounds = map->nodes[0].bounds;

        gather_lights(arena, map);

        for (size_t entity_index = 0; entity_index < map->entity_count; entity_index++)
        {
            map_entity_t *e = &map->entities[entity_index];

            if (is_class(map, e, strlit("worldspawn")))
            {
                map->worldspawn = e;
                break;
            }
        }
    }

    return map;
}

bool is_class(map_t *map, map_entity_t *entity, string_t classname)
{
    return string_match(value_from_key(map, entity, strlit("classname")), classname);
}

string_t value_from_key(map_t *map, map_entity_t *entity, string_t key)
{
    string_t result = { 0 };

    for (size_t property_index = 0; property_index < entity->property_count; property_index++)
    {
        map_property_t *prop = &map->properties[entity->first_property + property_index];

        if (string_match(key, prop->key))
        {
            result = prop->val;
            break;
        }
    }

    return result;
}

int int_from_key(map_t *map, map_entity_t *entity, string_t key)
{
    string_t value = value_from_key(map, entity, key);

    int64_t result = 0;
    string_parse_int(&value, &result);

    return (int)result;
}

float float_from_key(map_t *map, map_entity_t *entity, string_t key)
{
    string_t value = value_from_key(map, entity, key);

    float result = 0.0f;
    string_parse_float(&value, &result);

    return result;
}

v3_t v3_from_key(map_t *map, map_entity_t *entity, string_t key)
{
    string_t value = value_from_key(map, entity, key);

    v3_t v3 = { 0 };

    string_parse_float(&value, &v3.x); 
    string_parse_float(&value, &v3.y); 
    string_parse_float(&value, &v3.z); 

    return v3;
}
