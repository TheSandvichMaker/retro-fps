#include "common.h"

#include "string.h"
#include "string_list.h"
#include "arena.h"

//
//
//

void slist_appends_nocopy(string_list_t *list, arena_t *arena, string_t string)
{
    string_node_t *node = m_alloc_struct(arena, string_node_t);
    node->string = string;

    dll_push_back(list->first, list->last, node);
}

void slist_prepends_nocopy(string_list_t *list, arena_t *arena, string_t string)
{
    string_node_t *node = m_alloc_struct(arena, string_node_t);
    node->string = string;

    dll_push_front(list->first, list->last, node);
}

void slist_interleaves_nocopy(string_list_t *list, arena_t *arena, string_t string)
{
    if (list->first)
    {
        for (string_node_t *insert_after = list->first, *next; insert_after->next; insert_after = next)
        {
            next = insert_after->next;

            string_node_t *node = m_alloc_struct(arena, string_node_t);
            node->string = string;

            dll_insert_after(list->first, list->last, insert_after, node);
        }
    }
}

//

void slist_appends(string_list_t *list, arena_t *arena, string_t string)
{
    slist_appends_nocopy(list, arena, string_copy(arena, string));
}

void slist_appendf(string_list_t *list, arena_t *arena, const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    slist_appendf_va(list, arena, fmt, args);
    va_end(args);
}

void slist_appendf_va(string_list_t *list, arena_t *arena, const char *fmt, va_list args)
{
    string_t string = string_format_va(arena, fmt, args);
    slist_appends_nocopy(list, arena, string);
}

//

void slist_prepends(string_list_t *list, arena_t *arena, string_t string)
{
    slist_prepends_nocopy(list, arena, string_copy(arena, string));
}

void slist_prependf(string_list_t *list, arena_t *arena, const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    slist_prependf_va(list, arena, fmt, args);
    va_end(args);
}

void slist_prependf_va(string_list_t *list, arena_t *arena, const char *fmt, va_list args)
{
    string_t string = string_format_va(arena, fmt, args);
    slist_prepends_nocopy(list, arena, string);
}

//

void slist_interleaves(string_list_t *list, arena_t *arena, string_t string)
{
    slist_interleaves_nocopy(list, arena, string_copy(arena, string));
}

void slist_interleavef(string_list_t *list, arena_t *arena, const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    slist_interleavef_va(list, arena, fmt, args);
    va_end(args);
}

void slist_interleavef_va(string_list_t *list, arena_t *arena, const char *fmt, va_list args)
{
    string_t string = string_format_va(arena, fmt, args);
    slist_interleaves_nocopy(list, arena, string);
}

//

size_t slist_get_total_count(string_list_t *list)
{
    size_t total_count = 0;
    for (string_node_t *node = list->first; node; node = node->next)
    {
        total_count += node->string.count;
    }
    return total_count;
}

// TODO: this is literally the same thing as dynamic_string_t. deduplicate.
typedef struct string_buffer_t
{
    char  *data;
    size_t at;
    size_t capacity;
} string_buffer_t;

fn_local string_t string_buffer_push(string_buffer_t *buf, string_t string)
{
    string_t result = strnull;

    size_t size_left = buf->capacity - buf->at;
    if (size_left > 0)
    {
        size_t copy_size = MIN(size_left, string.count);
        copy_memory(&buf->data[buf->at], string.data, copy_size);

        result.data  = &buf->data[buf->at];
        result.count = copy_size;

        buf->at += copy_size;
    }

    return result;
}

fn_local string_t string_from_string_buffer(string_buffer_t *buf)
{
    string_t result = {
        .data  = buf->data,
        .count = buf->at,
    };
    return result;
}

string_t slist_flatten_into_with_separator(string_list_t          *list, 
                                           size_t                  buffer_size, 
                                           void                   *buffer, 
                                           string_t                separator, 
                                           slist_separator_flags_t flags)
{
    string_buffer_t buf = {
        .data     = buffer,
        .capacity = buffer_size,
    };

    if (flags & SLIST_SEPARATOR_BEFORE_FIRST)
    {
        string_buffer_push(&buf, separator);
    }

    for (string_node_t *node = list->first; node; node = node->next)
    {
        string_buffer_push(&buf, node->string);

        if (node->next)
        {
            string_buffer_push(&buf, separator);
        }
    }

    if (flags & SLIST_SEPARATOR_AFTER_LAST)
    {
        string_buffer_push(&buf, separator);
    }

    string_t result = string_from_string_buffer(&buf);
    return result;
}

string_t slist_flatten_into(string_list_t *list, size_t buffer_size, void *buffer)
{
    string_buffer_t buf = {
        .data     = buffer,
        .capacity = buffer_size,
    };

    for (string_node_t *node = list->first; node; node = node->next)
    {
        string_buffer_push(&buf, node->string);
    }

    string_t result = string_from_string_buffer(&buf);
    return result;
}

string_t slist_flatten_with_separator(string_list_t *list, arena_t *arena, string_t separator, slist_separator_flags_t flags)
{
    size_t total_count = 0;

    if (flags & SLIST_SEPARATOR_BEFORE_FIRST)
    {
        total_count += separator.count;
    }

    for (string_node_t *node = list->first; node; node = node->next)
    {
        total_count += node->string.count;

        if (node->next)
        {
            total_count += separator.count;
        }
    }

    if (flags & SLIST_SEPARATOR_AFTER_LAST)
    {
        total_count += separator.count;
    }

    char    *buffer = m_alloc_string(arena, total_count);
    string_t result = slist_flatten_into_with_separator(list, total_count, buffer, separator, flags);
    return result;
}

string_t slist_flatten(string_list_t *list, arena_t *arena)
{
    size_t   total_count = slist_get_total_count(list);
    char    *buffer      = m_alloc_string(arena, total_count);
    string_t result      = slist_flatten_into(list, total_count, buffer);
    return result;
}

string_t slist_flatten_null_terminated(string_list_t *list, arena_t *arena)
{
    size_t   total_count = slist_get_total_count(list) + 1;
    char    *buffer      = m_alloc_string(arena, total_count);
    string_t result      = slist_flatten_into(list, total_count, buffer);

    result.data[total_count] = 0;
    return result;
}

//

bool slist_contains_node(string_list_t *list, string_node_t *node)
{
    bool found_node = false;
    for (const string_node_t *test_node = list->first; test_node; test_node = test_node->next)
    {
        if (node == test_node)
        {
            found_node = true;
            break;
        }
    }
    return found_node;
}

void slist_remove(string_list_t *list, string_node_t *node)
{
#if DREAM_SLOW
    if (ALWAYS(slist_contains_node(list, node)))
    {
#endif
        dll_remove(list->first, list->last, node);
#if DREAM_SLOW
    }
#endif
}

string_node_t *slist_pop_last(string_list_t *list)
{
    string_node_t *result = NULL;
    if (!slist_empty(list))
    {
        result = list->last;
        dll_remove(list->first, list->last, list->last);
    }
    return result;
}

string_node_t *slist_find_node(string_list_t *list, string_t string)
{
    string_node_t *result = NULL;
    for (string_node_t *node = list->first; node; node = node->next)
    {
        if (string_match(node->string, string))
        {
            result = node;
        }
    }

    return result;
}

bool slist_find_and_remove(string_list_t *list, string_t string)
{
    string_node_t *node = slist_find_node(list, string);
    if (node)
    {
        // avoiding slist_remove to not do redudant debug checks of node-existence
        dll_remove(list->first, list->last, node);
    }

    return false;
}

bool slist_append_unique(string_list_t *list, arena_t *arena, string_t string)
{
    bool unique = true;
    for (string_node_t *node = list->first; node; node = node->next)
    {
        if (string_match(node->string, string))
        {
            unique = false;
            break;
        }
    }

    if (unique)
    {
        slist_appends(list, arena, string);
    }

    return unique;
}

void slist_split(string_list_t *list, arena_t *arena, string_t string, string_t separator)
{
    while (string.count > 0)
    {
        string_t segment = string_split(&string, separator);
        slist_appends(list, arena, segment);
    }
}

void slist_split_words(string_list_t *list, arena_t *arena, string_t string)
{
    while (string.count > 0)
    {
        string_t segment = string_split_word(&string);
        slist_appends(list, arena, segment);
    }
}

void slist_split_lines(string_list_t *list, arena_t *arena, string_t string)
{
    while (string.count > 0)
    {
        string_t segment = string_split_line(&string);
        slist_appends(list, arena, segment);
    }
}

string_list_t slist_from_array(arena_t *arena, size_t count, string_t *array)
{
    string_list_t list = { 0 };

    for (size_t i = 0; i < count; i++)
    {
        slist_appends(&list, arena, array[i]);
    }

    return list;
}
