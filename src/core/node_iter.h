#ifndef NODE_ITER_H
#define NODE_ITER_H

static inline void *node_next_depth_first_pre_order(void *node_void)
{
    node_t *node = node_void;
    node_t *next = NULL;
    
    if (node->first_child)
    {
        next = node->first_child;
    }
    else
    {
        node_t *p = node;

        while (p)
        {
            if (p->next)
            {
                next = p->next;
                break;
            }
            
            p = p->parent;
        }
    }

    return next;
}

#endif /* NODE_ITER_H */
