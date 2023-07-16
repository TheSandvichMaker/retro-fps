#ifndef WIN32_D3D11_H
#define WIN32_D3D11_H

#include "core/core.h"

#include "dream/render.h" // hm

int init_d3d11(void *hwnd);
void d3d11_draw_list(r_list_t *list, int width, int height);
void d3d11_present(void);

render_api_i *d3d11_get_api(void);

#endif /* WIN32_D3D11_H */
