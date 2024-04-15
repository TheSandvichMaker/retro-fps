// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#ifdef NO_STD_LIB
extern inline float logf(float x);

extern inline float expf(float x);
extern inline float sinf(float x);
extern inline float cosf(float x);
extern inline float tanf(float x);
extern inline float sqrtf(float x);
extern inline float fmodf(float x, float y);
extern inline float fabsf(float x);
#endif

void fatal_math_error(const char *message)
{
    FATAL_ERROR(message);
}

fn_local void mat_swap(mat_t *m, int row_a, int row_b)
{
    for (int j = 0; j < m->m; j++)
    {
        SWAP(float, M(m, row_a, j), M(m, row_b, j));
    }
}

fn_local void mat_scale(mat_t *m, int row, float scale)
{
    for (int j = 0; j < m->m; j++)
    {
        M(m, row, j) *= scale;
    }
}

fn_local void mat_add(mat_t *m, int row_a, int row_b, float scale)
{
    for (int j = 0; j < m->m; j++)
    {
        M(m, row_b, j) += M(m, row_a, j)*scale;
    }
}

fn_local int find_max_pivot(mat_t *m, int k)
{
    int result = k;
    float max = abs_ss(M(m, k, k));

    for (int i = k + 1; i < m->n; i++)
    {
        float val = abs_ss(M(m, i, k));
        if (val > max)
        {
            result = i;
        }
    }

    return result;
}

fn_local void back_substitute(mat_t *m, float *x)
{
    zero_array(x, m->n);

    for (int i = m->n - 1; i >= 0; i--)
    {
        float s = 0.0f;
        for (int j = i + 1; j < m->n; j++)
        {
            s += M(m, i, j)*x[j];
        }
        x[i] = M(m, i, m->n) - s;
    }
}

bool solve_system_of_equations(mat_t *m, float *x)
{
    if (m->m <= 0 || m->n <= 0)
        FATAL_ERROR("matrices need rows, columns...");

    if (m->m != m->n + 1)
        FATAL_ERROR("for gaussian substitution, expected a m*n matrix where m = n + 1");

    for (int k = 0; k < 3; k++)
    {
        mat_swap(m, k, find_max_pivot(m, k));

        if (abs_ss(M(m, k, k)) < 0.0001f)
            return false;

        mat_scale(m, k, 1.0f / M(m, k, k));

        for (int i = k + 1; i < 3; i++)
        {
            mat_add(m, k, i, -M(m, i, k));
        }
    }

    back_substitute(m, x);

    return true;
}
