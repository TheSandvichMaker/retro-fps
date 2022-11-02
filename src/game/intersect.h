#ifndef INTERSECT_H
#define INTERSECT_H

#include "core/api_types.h"

float ray_intersect_rect3(v3_t o, v3_t d, rect3_t rect);
v3_t get_normal_rect3(v3_t hit_p, rect3_t rect);

#endif /* INTERSECT_H */
