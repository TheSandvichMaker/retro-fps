#ifndef STRING_LIST_H
#define STRING_LIST_H

#include "api_types.h"

typedef struct string_node_t
{
    string_t string;

    struct string_node_t *next;
    struct string_node_t *prev;
} string_node_t;

typedef struct string_list_t
{
    string_node_t *first, *last;
} string_list_t;

// add string to the end of the list
void slist_appends       (string_list_t *list, arena_t *arena, string_t string);
void slist_appendf       (string_list_t *list, arena_t *arena, const char *fmt, ...);
void slist_appendf_va    (string_list_t *list, arena_t *arena, const char *fmt, va_list args);

// add string to the start of the list
void slist_prepends      (string_list_t *list, arena_t *arena, string_t string);
void slist_prependf      (string_list_t *list, arena_t *arena, const char *fmt, ...);
void slist_prependf_va   (string_list_t *list, arena_t *arena, const char *fmt, va_list args);

// add string inbetween nodes in the list (acting as a separator)
void slist_interleaves   (string_list_t *list, arena_t *arena, string_t string);
void slist_interleavef   (string_list_t *list, arena_t *arena, const char *fmt, ...);
void slist_interleavef_va(string_list_t *list, arena_t *arena, const char *fmt, va_list args);

// returns a flattened string of the contents of the list
string_t slist_flatten(string_list_t *list, arena_t *arena);
// returns a flattened string of the contents of the list as a C string
char *slist_flatten_null_terminated(string_list_t *list, arena_t *arena);

// uses linear search, not fast, just convenience. removes one instance of the string, not all
bool slist_remove(string_list_t *list, string_t string);
// uses linear search, not fast, just convenience
bool slist_append_unique(string_list_t *list, arena_t *arena, string_t string);

void slist_split(string_list_t *list, arena_t *arena, string_t string, string_t separator);
void slist_split_words(string_list_t *list, arena_t *arena, string_t string);
void slist_split_lines(string_list_t *list, arena_t *arena, string_t string);

string_list_t slist_from_array(arena_t *arena, size_t count, string_t *array);

#endif /* STRING_LIST_H */
