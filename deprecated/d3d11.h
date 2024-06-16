// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

int init_d3d11(void *hwnd);
void d3d11_execute_command_buffer(struct r_command_buffer_t *commands, int width, int height);
void d3d11_present(void);

struct render_api_i *d3d11_get_api(void);
