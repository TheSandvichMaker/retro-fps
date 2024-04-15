#ifndef STRING_LIST_H
#define STRING_LIST_H

#include "api_types.h"

fn_local bool slist_empty(string_list_t *list)
{
    bool result = !list->first;
    DEBUG_ASSERT(!list->first == !list->last);
    return result;
}

typedef uint32_t slist_separator_flags_t;
typedef enum slist_separator_flags_enum_t
{
    SLIST_SEPARATOR_BEFORE_FIRST = 1 << 0,
    SLIST_SEPARATOR_AFTER_LAST   = 1 << 1,
} slist_separator_flags_enum_t;

// add string to the end of the list
fn void slist_appends_nocopy(string_list_t *list, arena_t *arena, string_t string);
fn void slist_appends       (string_list_t *list, arena_t *arena, string_t string);
fn void slist_appendf       (string_list_t *list, arena_t *arena, const char *fmt, ...);
fn void slist_appendf_va    (string_list_t *list, arena_t *arena, const char *fmt, va_list args);

// add string to the start of the list
fn void slist_prepends_nocopy(string_list_t *list, arena_t *arena, string_t string);
fn void slist_prepends      (string_list_t *list, arena_t *arena, string_t string);
fn void slist_prependf      (string_list_t *list, arena_t *arena, const char *fmt, ...);
fn void slist_prependf_va   (string_list_t *list, arena_t *arena, const char *fmt, va_list args);

// add string inbetween nodes in the list (acting as a separator)
fn void slist_interleaves_nocopy(string_list_t *list, arena_t *arena, string_t string);
fn void slist_interleaves   (string_list_t *list, arena_t *arena, string_t string);
fn void slist_interleavef   (string_list_t *list, arena_t *arena, const char *fmt, ...);
fn void slist_interleavef_va(string_list_t *list, arena_t *arena, const char *fmt, va_list args);

fn size_t slist_get_total_count(string_list_t *list);

// returns a flattened string of the contents of the list
fn string_t slist_flatten_into_with_separator(string_list_t *list, size_t buffer_size, void *buffer, string_t separator, slist_separator_flags_t flags);
fn string_t slist_flatten_into(string_list_t *list, size_t buffer_size, void *buffer);
fn string_t slist_flatten_with_separator(string_list_t *list, arena_t *arena, string_t separator, slist_separator_flags_t flags);
fn string_t slist_flatten(string_list_t *list, arena_t *arena);
// returns a flattened string of the contents of the list as a C string
fn string_t slist_flatten_null_terminated(string_list_t *list, arena_t *arena);

fn bool slist_contains_node(string_list_t *list, string_node_t *node);
fn void slist_remove(string_list_t *list, string_node_t *node);
fn string_node_t *slist_pop_last(string_list_t *list);
// uses linear search, not fast, just convenience. removes one instance of the string, not all
fn string_node_t *slist_find_node(string_list_t *list, string_t string);
fn bool slist_find_and_remove(string_list_t *list, string_t string);
// uses linear search, not fast, just convenience
fn bool slist_append_unique(string_list_t *list, arena_t *arena, string_t string);

fn void slist_split(string_list_t *list, arena_t *arena, string_t string, string_t separator);
fn void slist_split_words(string_list_t *list, arena_t *arena, string_t string);
fn void slist_split_lines(string_list_t *list, arena_t *arena, string_t string);

fn string_list_t slist_from_array(arena_t *arena, size_t count, string_t *array);

#endif /* STRING_LIST_H */
