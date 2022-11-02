#ifndef DIAGRAM_H
#define DIAGRAM_H

#include "core/api_types.h"

#define DIAG_NODE_MAX_NAME_SIZE (64)

typedef enum diag_kind_t
{
    DIAG_GROUP,
    DIAG_ARROW,
    DIAG_BOX,
} diag_kind_t;

typedef struct diag_node_t
{
    struct diag_node_t *next;
    struct diag_node_t *first_child, *last_child;

    STRING_STORAGE(DIAG_NODE_MAX_NAME_SIZE) name;

    diag_kind_t kind;
    union
    {
        struct
        {
            v3_t start;
            v3_t end;
        };
        rect3_t bounds;
    };
    v3_t color;
} diag_node_t;

diag_node_t *diag_begin(string_t name);

diag_node_t *diag_add_group(diag_node_t *parent, string_t name);
diag_node_t *diag_add_arrow(diag_node_t *parent, v3_t color, v3_t start, v3_t end);
diag_node_t *diag_add_box(diag_node_t *parent, v3_t color, rect3_t bounds);

void diag_set_name(diag_node_t *node, string_t name);

void diag_draw_all(void);

#endif /* DIAGRAM_H */
