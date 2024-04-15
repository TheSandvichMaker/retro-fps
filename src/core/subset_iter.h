// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#ifndef SUBSET_ITER_H
#define SUBSET_ITER_H

typedef struct subset_iter_t
{
    int i;
    int k;
    size_t n;
    size_t *indices;
} subset_iter_t;

// algorithm is split into two parts to work with a for loop
// if this was a co-routine there would be a yield inbetween
// the pre and post blocks

fn_local void subset__pre(subset_iter_t *iter)
{
    if (iter->i < 0) return;

    int j;

    for (j = iter->i + 1; j < iter->k; j++)
    {
        iter->indices[j] = iter->indices[j - 1] + 1;
    }

    iter->i = j - 1;
}

fn_local void subset__post(subset_iter_t *iter)
{
    while (iter->indices[iter->i] == iter->i + iter->n - iter->k)
    {
        iter->i -= 1;
    }

    if (iter->i >= 0)
        iter->indices[iter->i] += 1;
}

fn_local subset_iter_t iterate_subsets(arena_t *arena, size_t n, int k)
{
    subset_iter_t iter = {
        .i = n >= k ? 0 : -1,
        .k = k,
        .n = n,
        .indices = m_alloc_array(arena, k, size_t),
    };

    subset__pre(&iter);
    return iter;
}

fn_local bool subset_valid(subset_iter_t *iter)
{
    return iter->i >= 0;
}

fn_local void subset_next(subset_iter_t *iter)
{
    subset__post(iter);
    subset__pre(iter);
}

#endif /* SUBSET_ITER_H */
