#pragma once

#include "core/api_types.h"

DEFINE_HANDLE_TYPE(texture_handle_t);
DEFINE_HANDLE_TYPE(mesh_handle_t);

#define NULL_TEXTURE_HANDLE ((texture_handle_t){0})
#define NULL_MESH_HANDLE ((mesh_handle_t){0})

typedef unsigned char r_view_index_t;
