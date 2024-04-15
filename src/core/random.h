// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

fn_local uint32_t random_uint32(random_series_t *r)
{
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return r->state;
}

// returns random [0, 1) float
fn_local float random_unilateral (random_series_t *r)
{
    // NOTE: Stolen from rnd.h, courtesy of Jonatan Hedborg
    uint32_t exponent = 127;
    uint32_t mantissa = random_uint32(r) >> 9;
    uint32_t bits = (exponent << 23) | mantissa;
    float result = *(float *)&bits - 1.0f;
    return result;
}

// returns random [-1, 1) float
fn_local float random_bilateral(random_series_t *r)
{
    return -1.0f + 2.0f*random_unilateral(r);
}

// returns random number in range [0, range)
fn_local uint32_t random_choice(random_series_t *r, uint32_t range)
{
    uint32_t result = random_uint32(r) % range;
    return result;
}

// returns random number in range [1, sides]
fn_local uint32_t dice_roll(random_series_t *r, uint32_t sides)
{
    uint32_t result = 1 + random_choice(r, sides);
    return result;
}

// returns random number in range [min, max]
fn_local int32_t random_range_i32(random_series_t *r, int32_t min, int32_t max)
{
    if (max < min)
        max = min;

    int32_t result = min + (int32_t)random_uint32(r) % (max - min + 1);
    return result;
}

// returns random float in range [min, max)
fn_local float random_range_f32(random_series_t *r, float min, float max)
{
    float range = random_unilateral(r);
    float result = min + range*(max - min);
    return result;
}

fn_local v2_t random_unilateral2(random_series_t *r)
{
    v2_t result = {
        random_unilateral(r),
        random_unilateral(r),
    };
    return result;
}

fn_local v3_t random_unilateral3(random_series_t *r)
{
    v3_t result = {
        random_unilateral(r),
        random_unilateral(r),
        random_unilateral(r),
    };
    return result;
}

fn_local v2_t random_in_unit_square(random_series_t *r)
{
    v2_t result = {
        random_bilateral(r),
        random_bilateral(r),
    };
    return result;
}

fn_local v3_t random_in_unit_cube(random_series_t *r)
{
    v3_t result = {
        random_bilateral(r),
        random_bilateral(r),
        random_bilateral(r),
    };
    return result;
}

fn_local v2_t random_in_unit_disk(random_series_t *r)
{
    v2_t result;
    for (;;)
    {
        result = random_in_unit_square(r);
        if (dot(result, result) < 1.0)
        {
            break;
        }
    }
    return result;
}

fn_local v3_t random_in_unit_sphere(random_series_t *r)
{
    v3_t result;
    for (;;)
    {
        result = random_in_unit_cube(r);
        if (dot(result, result) < 1.0)
        {
            break;
        }
    }
    return result;
}
