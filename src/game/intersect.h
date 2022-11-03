#ifndef INTERSECT_H
#define INTERSECT_H

#include "core/api_types.h"

float ray_intersect_rect3(v3_t o, v3_t d, rect3_t rect);
float ray_intersect_triangle(v3_t o, v3_t d, v3_t a, v3_t b, v3_t c, v3_t *uvw);

v3_t get_normal_rect3(v3_t hit_p, rect3_t rect);

#endif /* INTERSECT_H */
