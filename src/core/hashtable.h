#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "api_types.h"

enum { UNUSED_KEY_VALUE = 0 };

typedef struct hash_entry_t
{
    uint64_t key;
    uint64_t value;
} hash_entry_t;

typedef struct hash_t
{
    uint32_t mask;
    uint32_t load;
    hash_entry_t *entries;
} hash_t;

bool hash_find(const hash_t *table, uint64_t key, uint64_t *value);
void hash_add (      hash_t *table, uint64_t key, uint64_t  value);

bool hash_find_ptr(const hash_t *table, uint64_t key, void **value);
void hash_add_ptr (      hash_t *table, uint64_t key, void  *value);

typedef struct hash_iter_t
{
    // internal state
    const hash_t *table;
    uint64_t index;

    // iterator result
    union
    {
        uint64_t value;
        void *ptr;
    };
} hash_iter_t;

static inline hash_iter_t hash_iter(const hash_t *table)
{
    hash_iter_t it = {
        .table = table,
        .index = 0,
    };

    return it;
}

static inline bool hash_iter_next(hash_iter_t *it)
{
    bool result = false;

    const hash_t *table = it->table;
    while (it->index <= table->mask)
    {
        if (table->entries[it->index].key != UNUSED_KEY_VALUE)
        {
            result = true;

            it->value = table->entries[it->index].value;
            it->index += 1;

            break;
        }

        it->index += 1;
    }

    return result;
}

#endif /* HASHTABLE_H */
