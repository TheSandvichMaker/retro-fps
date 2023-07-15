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
    for (string_node_t *insert_after = list->first, *next; insert_after->next; insert_after = next)
    {
        next = insert_after->next;

        string_node_t *node = m_alloc_struct(arena, string_node_t);
        node->string = string;

        dll_insert_after(list->first, list->last, insert_after, node);
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

string_t slist_flatten(string_list_t *list, arena_t *arena)
{
    size_t total_count = 0;
    for (string_node_t *node = list->first; node; node = node->next)
    {
        total_count += node->string.count;
    }

    char *data = m_alloc_string(arena, total_count);
    char *at   = data;

    for (string_node_t *node = list->first; node; node = node->next)
    {
        copy_memory(at, node->string.data, node->string.count);
        at += node->string.count;
    }

    string_t string = { total_count, data };
    return string;
}

char *slist_flatten_null_terminated(string_list_t *list, arena_t *arena)
{
    size_t total_count = 0;
    for (string_node_t *node = list->first; node; node = node->next)
    {
        total_count += node->string.count;
    }

    char *data = m_alloc_string(arena, total_count + 1);
    char *at   = data;

    for (string_node_t *node = list->first; node; node = node->next)
    {
        copy_memory(at, node->string.data, node->string.count);
        at += node->string.count;
    }

    data[total_count] = 0;
    return data;
}

//

bool slist_remove(string_list_t *list, string_t string)
{
    for (string_node_t *node = list->first, *next; node; node = next)
    {
        next = node->next;

        if (string_match(node->string, string))
        {
            dll_remove(list->first, list->last, node);
            return true;
        }
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
