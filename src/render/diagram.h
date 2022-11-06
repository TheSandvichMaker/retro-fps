#ifndef DIAGRAM_H
#define DIAGRAM_H

#include "core/api_types.h"

#define DIAG_NODE_MAX_NAME_SIZE (64)

typedef struct bitmap_font_t bitmap_font_t;

typedef enum diag_kind_t
{
    DIAG_GROUP,
    DIAG_ARROW,
    DIAG_BOX,
    DIAG_TEXT,
} diag_kind_t;

// diagrams being able to persist across frames is a bad design. it does not spark joy.
// better to just persist whatever debug info required, and then draw diagrams from that.

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
            v3_t p0;
            v3_t p1;
        };
        rect3_t bounds;
    };
    v3_t color;
    string_t text;
} diag_node_t;

diag_node_t *diag_begin(string_t name);

diag_node_t *diag_add_group(diag_node_t *parent, string_t name);
diag_node_t *diag_add_arrow(diag_node_t *parent, v3_t color, v3_t start, v3_t end);
diag_node_t *diag_add_box(diag_node_t *parent, v3_t color, rect3_t bounds);
diag_node_t *diag_add_text(diag_node_t *parent, v3_t color, v3_t position, string_t text);

void diag_set_name(diag_node_t *node, string_t name);

void diag_draw_all(const bitmap_font_t *font);

#endif /* DIAGRAM_H */
