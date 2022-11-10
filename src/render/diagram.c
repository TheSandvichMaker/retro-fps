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

diag_node_t *diag_add_arrow(diag_node_t *parent, uint32_t color, v3_t start, v3_t end)
{
    diag_node_t *diag = diag_new_child(parent, DIAG_ARROW);
    diag->color = color;
    diag->p0 = start;
    diag->p1 = end;
    return diag;
}

diag_node_t *diag_add_box(diag_node_t *parent, uint32_t color, rect3_t bounds)
{
    diag_node_t *diag = diag_new_child(parent, DIAG_BOX);
    diag->color  = color;
    diag->bounds = bounds;
    return diag;
}

diag_node_t *diag_add_text(diag_node_t *parent, uint32_t color, v3_t position, string_t text)
{
    // TODO: text lifetime
    diag_node_t *diag = diag_new_child(parent, DIAG_TEXT);
    diag->color = color;
    diag->text  = text;
    diag->p0 = position;
    return diag;
}

static void diag_draw_children(diag_node_t *parent, const bitmap_font_t *font)
{
    diag_node_t *prev = NULL;
    for (diag_node_t *diag = parent->first_child; diag; diag = diag->next)
    {
        switch (diag->kind)
        {
            case DIAG_GROUP:
            {
                diag_draw_children(diag, font);
            } break;

            case DIAG_ARROW:
            {
                r_immediate_arrow(diag->p0, diag->p1, diag->color);
            } break;

            case DIAG_BOX:
            {
                r_immediate_box(diag->bounds, diag->color);
            } break;

            case DIAG_TEXT:
            {
                r_view_t *view = r_get_top_view();

                v3_t p = r_to_view_space(view, diag->p0, 1);
                if (p.z > 0.0f)
                {
                    r_immediate_text(font, p.xy, diag->color, diag->text);
                }
            } break;
        }

        prev = diag;
    }
}

void diag_draw_all(const bitmap_font_t *font)
{
    r_immediate_topology(R_PRIMITIVE_TOPOLOGY_LINELIST);
    r_immediate_depth_test(true);
    for (hash_iter_t it = hash_iter(&g_diag_state.hash); hash_iter_next(&it);)
    {
        diag_node_t *root = it.ptr;
        diag_draw_children(root, font);
    }
    r_immediate_flush();
}

void diag_set_name(diag_node_t *node, string_t name)
{
    STRING_INTO_STORAGE(node->name, name);
}
