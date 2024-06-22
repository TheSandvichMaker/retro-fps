// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

enum { TABLE_UNUSED_KEY_VALUE = 0 };

enum { TABLE_RESERVE_CAPACITY = 1 << 20, TABLE_HALF_CAPACITY = 1 << 19 }; // up to ~500k entries
enum { TABLE_SECONDARY_BUFFER = 1 << 0, TABLE_EXTERNAL_MEMORY = 1 << 1 };

#define   TABLE_TAG_POINTER(pointer, tags) (void *)((uintptr_t)(pointer)|(tags))
#define TABLE_UNTAG_POINTER(pointer)       (void *)((uintptr_t)(pointer) & ~(alignof(table_entry_t) - 1))
#define TABLE_POINTER_HAS_TAGS(pointer, tags) !!((uintptr_t)(pointer) & tags)

typedef struct table_entry_t
{
    uint64_t key;
    uint64_t value;
} table_entry_t;

typedef struct table_t
{
    uint32_t mask;
    uint32_t load;
    table_entry_t *entries;
} table_t;

fn bool table_find_entry    (const table_t *table, uint64_t key, uint64_t **value);
fn bool table_find          (const table_t *table, uint64_t key, uint64_t  *value);
fn void table_insert        (      table_t *table, uint64_t key, uint64_t   value);
fn bool table_remove        (      table_t *table, uint64_t key);

fn void *table_find_object  (const table_t *table, uint64_t key);
fn void  table_insert_object(      table_t *table, uint64_t key, void *value);

fn void table_release(table_t *table);

fn_local table_entry_t *table_get_entries(const table_t *table)
{
	return TABLE_UNTAG_POINTER(table->entries);
}

typedef struct table_iter_t
{
    // internal state
    const table_t *table;
    uint64_t internal_index;

	uint64_t i;
	uint64_t key;

    // iterator result
    union
    {
        uint64_t value;
        void *ptr;
    };
} table_iter_t;

fn_local table_iter_t table_iter(const table_t *table)
{
    table_iter_t it = {
        .table = table,
		.i = (uint64_t)-1,
    };

    return it;
}

fn_local bool table_iter_next(table_iter_t *it)
{
    bool result = false;

    const table_t *table = it->table;

    if (!table->entries)
        return false;

	table_entry_t *entries = table_get_entries(table);

    while (it->internal_index <= table->mask)
    {
        if (entries[it->internal_index].key != TABLE_UNUSED_KEY_VALUE)
        {
            result = true;

			it->key   = entries[it->internal_index].key;
            it->value = entries[it->internal_index].value;
            it->internal_index += 1;

            break;
        }

        it->internal_index += 1;
    }

	it->i += 1;

    return result;
}
