// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

fn wchar_t *win32_format_error_wide(HRESULT error);
fn null_term_string_t win32_format_error(arena_t *arena, HRESULT error);
fn void win32_display_last_error(void);
fn void win32_output_last_error(string16_t prefix);
fn void win32_error_box(char *message);
fn void win32_hresult_error_box(HRESULT hr, const char *message, ...);
fn void win32_hresult_error_box_va(HRESULT hr, const char *message, va_list args);
