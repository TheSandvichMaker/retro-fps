// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

fn size_t string_count(const char *string);
fn size_t string16_count(const wchar_t *string);

fn bool string_empty(string_t string);

fn string_t             string_copy            (arena_t *arena, string_t string);
fn string_t             string_copy_cstr       (arena_t *arena, const char *string);

fn null_term_string_t   string_from_cstr       (char    *string);
fn null_term_string16_t string16_from_cstr     (wchar_t *string);

fn null_term_string_t   string_null_terminate  (arena_t *arena, string_t string);
fn null_term_string16_t string16_null_terminate(arena_t *arena, string16_t string);

fn null_term_string_t   string_format          (arena_t *arena, const char *fmt, ...);
fn null_term_string_t   string_format_va       (arena_t *arena, const char *fmt, va_list args);

#define Sf(fmt, ...) string_format(m_get_temp(NULL, 0), fmt, ##__VA_ARGS__)

fn string_t string_format_into_buffer   (char *buffer, size_t buffer_size, const char *fmt, ...);
fn string_t string_format_into_buffer_va(char *buffer, size_t buffer_size, const char *fmt, va_list args);

// null_term_string16_t string16_format   (arena_t *arena, const wchar_t *fmt, ...);
// null_term_string16_t string16_format_va(arena_t *arena, const wchar_t *fmt, va_list args);

#define STRING_NPOS SIZE_MAX

// TODO: remove from header
fn_local char string_peek(string_t string, size_t index)
{
    if (index > string.count)
        return 0;

    return string.data[index];
}

fn string_t substring(string_t string, size_t first, size_t count);

fn size_t string_find_first_char(string_t string, char c);
fn size_t string_find_last_char(string_t string, char c);

fn size_t string_find_first(string_t string, string_t needle);

fn string_t string_extension(string_t string);
fn string_t string_strip_extension(string_t string);

fn string_t string_normalize_path(arena_t *arena, string_t path);
fn string_t string_path_leaf(string_t path);
fn string_t string_path_directory(string_t path);
fn bool string_path_strip_root(string_t path, string_t *out_root, string_t *out_remainder);

fn bool string_match(string_t a, string_t b);
fn bool string_match_nocase(string_t a, string_t b);
fn bool string_match_prefix(string_t string, string_t prefix);

// TODO: remove from header
fn_local bool is_whitespace(char c)
{
    return c == ' '  || 
           c == '\t' ||
           c == '\n' ||
           c == '\r';
}

fn_local bool is_newline(char c)
{
    return c == '\n' ||
           c == '\r';
}

fn_local bool is_alphabetic(char c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z'));
}

fn_local bool is_numeric(char c)
{
    return (c >= '0' && c <= '9');
}

extern char to_lowercase_table[256];

fn_local char to_lower(char c)
{
    return to_lowercase_table[(unsigned char)c];
}

fn string_t string_to_lower(arena_t *arena, string_t string);

// returns a string with the spaces trimmed, pointing into the original string
fn string_t string_trim_left_spaces(string_t string);
fn string_t string_trim_right_spaces(string_t string);
fn string_t string_trim_spaces(string_t string);

// advances the string by count
fn void string_skip(string_t *string, size_t count);

// split strings in various ways, always returning the left hand side and modifying the string to contain the right hand side of the split
fn string_t string_split(string_t *string, string_t separator);
fn string_t string_split_word(string_t *string);
fn string_t string_split_line(string_t *string);

fn bool string_parse_int(string_t *string, int64_t *value);
fn bool string_parse_float(string_t *string, float *value);

fn null_term_string16_t utf16_from_utf8(arena_t *arena, string_t   string);
fn null_term_string_t   utf8_from_utf16(arena_t *arena, string16_t string);

fn uint64_t string_hash(string_t string);
fn uint64_t string_hash_with_seed(string_t string, uint64_t seed);

typedef struct string_storage_overlay_t
{
	size_t count;
	char data[1];
} string_storage_overlay_t;

fn void string_storage_append_impl(string_storage_overlay_t *storage, size_t capacity, string_t string);
#define string_storage_append(storage, string) (string_storage_append_impl((string_storage_overlay_t *)(storage), ARRAY_COUNT((storage)->data), string))
