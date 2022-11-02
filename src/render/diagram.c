#include "diagram.h"
#include "render.h"
#include "core/string.h"
#include "core/node_iter.h"
#include "core/hashtable.h"

typedef struct diag_state_t
{
    arena_t arena;
    hash_t hash;

    diag_node_t *first_free_node;
} diag_state_t;

static diag_state_t g_diag_state;

static void diag_free(diag_node_t *node);
static void diag_free_children(diag_node_t *node)
{
    while (node->first_child)
    {
        diag_node_t *to_free = sll_pop(node->first_child);

        if (node->kind == DIAG_GROUP)
            diag_free_children(to_free);

        to_free->next = g_diag_state.first_free_node;
        g_diag_state.first_free_node = to_free;
    }
    node->last_child = NULL;
}

void diag_free(diag_node_t *node)
{
    diag_free_children(node);

    node->next = g_diag_state.first_free_node;
    g_diag_state.first_free_node = node;
}

static diag_node_t *diag_new_node(diag_kind_t kind)
{
    if (!g_diag_state.first_free_node)
    {
        g_diag_state.first_free_node = m_alloc_struct_nozero(&g_diag_state.arena, diag_node_t);
        g_diag_state.first_free_node->next = NULL;
    }

    diag_node_t *diag = sll_pop(g_diag_state.first_free_node);
    zero_struct(diag);

    diag->kind = kind;

    return diag;
}

static diag_node_t *diag_new_child(diag_node_t *parent, diag_kind_t kind)
{
    diag_node_t *result = diag_new_node(kind);
    sll_push_back(parent->first_child, parent->last_child, result);

    return result;
}

diag_node_t *diag_begin(string_t name)
{
    if (name.count > DIAG_NODE_MAX_NAME_SIZE)
        name.count = DIAG_NODE_MAX_NAME_SIZE;

    diag_node_t *root = NULL;
    if (hash_find_ptr(&g_diag_state.hash, string_hash(name), &root))
    {
        if (ALWAYS(string_match(STRING_FROM_STORAGE(root->name), name)))
        {
            diag_free_children(root);
        }
        else
        {
            root = NULL;
        }
    }

    if (!root)
    {
        root = diag_new_node(DIAG_GROUP);
        hash_add_ptr(&g_diag_state.hash, string_hash(name), root);
    }

    STRING_INTO_STORAGE(root->name, name);

    return root;
}

diag_node_t *diag_add_group(diag_node_t *parent, string_t name)
{
    diag_node_t *group = diag_new_child(parent, DIAG_GROUP);
    STRING_INTO_STORAGE(group->name, name);

    return group;
}

diag_node_t *diag_add_arrow(diag_node_t *parent, v3_t color, v3_t start, v3_t end)
{
    diag_node_t *diag = diag_new_child(parent, DIAG_ARROW);
    diag->color = color;
    diag->start = start;
    diag->end   = end;
    return diag;
}

diag_node_t *diag_add_box(diag_node_t *parent, v3_t color, rect3_t bounds)
{
    diag_node_t *diag = diag_new_child(parent, DIAG_BOX);
    diag->color  = color;
    diag->bounds = bounds;
    return diag;
}

static void diag_draw_children(diag_node_t *parent)
{
    diag_node_t *prev = NULL;
    for (diag_node_t *diag = parent->first_child; diag; diag = diag->next)
    {
        switch (diag->kind)
        {
            case DIAG_GROUP:
            {
                diag_draw_children(diag);
            } break;

            case DIAG_ARROW:
            {
                //float shaft_ratio = 0.95f;
                float head_size = 1.0f;

                v3_t  arrow_vector = sub(diag->end, diag->start);
                float arrow_length = vlen(arrow_vector);

                v3_t arrow_direction = normalize_or_zero(arrow_vector);

                float shaft_length = max(0.0f, arrow_length - 3.0f*head_size);

                v3_t shaft_vector = mul(shaft_length, arrow_direction);
                v3_t shaft_end    = add(diag->start, shaft_vector);

                v3_t t, b;
                get_tangent_vectors(arrow_direction, &t, &b);

                size_t arrow_segment_count = 8;
                for (size_t i = 0; i < arrow_segment_count; i++)
                {
                    float circ0 = 2.0f*PI32*((float)(i + 0) / (float)arrow_segment_count);
                    float circ1 = 2.0f*PI32*((float)(i + 1) / (float)arrow_segment_count);

                    float s0, c0;
                    sincos_ss(circ0, &s0, &c0);

                    float s1, c1;
                    sincos_ss(circ1, &s1, &c1);

                    v3_t v0 = add(shaft_end, add(mul(t, head_size*s0), mul(b, head_size*c0)));
                    v3_t v1 = add(shaft_end, add(mul(t, head_size*s1), mul(b, head_size*c1)));
                    r_immediate_line(v0, v1, diag->color);

                    r_immediate_line(v0, diag->end, diag->color);
                }

                r_immediate_line(diag->start, shaft_end, diag->color);
            } break;

            case DIAG_BOX:
            {
                v3_t v000 = { diag->bounds.min.x, diag->bounds.min.y, diag->bounds.min.z };
                v3_t v100 = { diag->bounds.max.x, diag->bounds.min.y, diag->bounds.min.z };
                v3_t v010 = { diag->bounds.min.x, diag->bounds.max.y, diag->bounds.min.z };
                v3_t v110 = { diag->bounds.max.x, diag->bounds.max.y, diag->bounds.min.z };

                v3_t v001 = { diag->bounds.min.x, diag->bounds.min.y, diag->bounds.max.z };
                v3_t v101 = { diag->bounds.max.x, diag->bounds.min.y, diag->bounds.max.z };
                v3_t v011 = { diag->bounds.min.x, diag->bounds.max.y, diag->bounds.max.z };
                v3_t v111 = { diag->bounds.max.x, diag->bounds.max.y, diag->bounds.max.z };

                // bottom plane
                r_immediate_line(v000, v100, diag->color);
                r_immediate_line(v100, v110, diag->color);
                r_immediate_line(v110, v010, diag->color);
                r_immediate_line(v010, v000, diag->color);

                // top plane
                r_immediate_line(v001, v101, diag->color);
                r_immediate_line(v101, v111, diag->color);
                r_immediate_line(v111, v011, diag->color);
                r_immediate_line(v011, v001, diag->color);

                // "pillars"
                r_immediate_line(v000, v001, diag->color);
                r_immediate_line(v100, v101, diag->color);
                r_immediate_line(v010, v011, diag->color);
                r_immediate_line(v110, v111, diag->color);
            } break;
        }

        prev = diag;
    }
}

void diag_draw_all(void)
{
    r_immediate_topology(R_PRIMITIVE_TOPOLOGY_LINELIST);
    r_immediate_depth_test(true);
    for (hash_iter_t it = hash_iter(&g_diag_state.hash); hash_iter_next(&it);)
    {
        diag_node_t *root = it.ptr;
        diag_draw_children(root);
    }
    r_immediate_flush();
}

void diag_set_name(diag_node_t *node, string_t name)
{
    STRING_INTO_STORAGE(node->name, name);
}
