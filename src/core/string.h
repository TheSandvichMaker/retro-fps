#ifndef DREAM_STRING_H
#define DREAM_STRING_H

#include "core/api_types.h"

size_t string_count(const char *string);
size_t string16_count(const wchar_t *string);

DREAM_INLINE bool string_empty(string_t string)
{
	return string.count == 0 || !string.data;
}

static inline string_t string_from_cstr(char *string)
{
    string_t result;
    result.count = string_count(string);
    result.data  = string;
    return result;
}

static inline string16_t string16_from_cstr(wchar_t *string)
{
    string16_t result;
    result.count = string16_count(string);
    result.data  = string;
    return result;
}

string_t string_copy(arena_t *arena, string_t string);
string_t string_copy_cstr(arena_t *arena, const char *string);
char *string_null_terminate(arena_t *arena, string_t string);
wchar_t *string16_null_terminate(arena_t *arena, string16_t string);

string_t string_format(arena_t *arena, const char *fmt, ...);
string_t string_format_va(arena_t *arena, const char *fmt, va_list args);

#define Sf(fmt, ...) string_format(m_get_temp(NULL, 0), fmt, ##__VA_ARGS__)

string_t string_format_into_buffer(char *buffer, size_t buffer_size, const char *fmt, ...);
string_t string_format_into_buffer_va(char *buffer, size_t buffer_size, const char *fmt, va_list args);

// string16_t string16_format(arena_t *arena, const wchar_t *fmt, ...);
// string16_t string16_format_va(arena_t *arena, const wchar_t *fmt, va_list args);

#define STRING_NPOS SIZE_MAX

static inline char string_peek(string_t string, size_t index)
{
    if (index > string.count)
        return 0;

    return string.data[index];
}

string_t substring(string_t string, size_t first, size_t count);

size_t string_find_first_char(string_t string, char c);
size_t string_find_last_char(string_t string, char c);

size_t string_find_first(string_t string, string_t needle);

string_t string_extension(string_t string);
string_t string_strip_extension(string_t string);

string_t string_normalize_path(arena_t *arena, string_t path);
string_t string_path_leaf(string_t path);
string_t string_path_directory(string_t path);
DREAM_GLOBAL bool string_path_strip_root(string_t path, string_t *out_root, string_t *out_remainder);

bool string_match(string_t a, string_t b);
bool string_match_nocase(string_t a, string_t b);
bool string_match_prefix(string_t string, string_t prefix);

static inline bool is_whitespace(char c)
{
    return c == ' '  || 
           c == '\t' ||
           c == '\n' ||
           c == '\r';
}

static inline bool is_newline(char c)
{
    return c == '\n' ||
           c == '\r';
}

static inline bool is_alphabetic(char c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z'));
}

static inline bool is_numeric(char c)
{
    return (c >= '0' && c <= '9');
}

extern char to_lowercase_table[256];

static inline char to_lower(char c)
{
    return to_lowercase_table[(unsigned char)c];
}

// returns a string with the spaces trimmed, pointing into the original string
string_t string_trim_left_spaces(string_t string);
string_t string_trim_right_spaces(string_t string);
string_t string_trim_spaces(string_t string);

// advances the string by count
void string_skip(string_t *string, size_t count);

// split strings in various ways, always returning the left hand side and modifying the string to contain the right hand side of the split
string_t string_split(string_t *string, string_t separator);
string_t string_split_word(string_t *string);
string_t string_split_line(string_t *string);

bool string_parse_int(string_t *string, int64_t *value);
bool string_parse_float(string_t *string, float *value);

string16_t utf16_from_utf8(arena_t *arena, string_t   string);
string_t   utf8_from_utf16(arena_t *arena, string16_t string);

uint64_t string_hash(string_t string);
uint64_t string_hash_with_seed(string_t string, uint64_t seed);

DREAM_GLOBAL void string_storage_append_impl(string_storage_overlay_t *storage, size_t capacity, string_t string);
#define string_storage_append(storage, string) (string_storage_append_impl((string_storage_overlay_t *)(storage), ARRAY_COUNT((storage)->data), string))

#endif /* DREAM_STRING_H */
