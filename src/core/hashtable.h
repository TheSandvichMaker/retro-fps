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

DREAM_API bool hash_find(const hash_t *table, uint64_t key, uint64_t *value);
DREAM_API void hash_add (      hash_t *table, uint64_t key, uint64_t  value);
DREAM_API bool hash_rem (      hash_t *table, uint64_t key);

DREAM_API bool hash_find_ptr(const hash_t *table, uint64_t key, void **value);
DREAM_API void hash_add_ptr (      hash_t *table, uint64_t key, void  *value);

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

DREAM_INLINE hash_iter_t hash_iter(const hash_t *table)
{
    hash_iter_t it = {
        .table = table,
        .index = 0,
    };

    return it;
}

DREAM_INLINE bool hash_iter_next(hash_iter_t *it)
{
    bool result = false;

    const hash_t *table = it->table;

    if (!table->entries)
        return false;

    while (it->index <= table->mask)
    {
		// FIXME: WRONGNESS ALERT. DOES NOT UNTAG THE ENTRIES POINTER. NOT GOOD, VERY BAD. DO NOT USE.
		// FIXME: WRONGNESS ALERT. DOES NOT UNTAG THE ENTRIES POINTER. NOT GOOD, VERY BAD. DO NOT USE.
		// FIXME: WRONGNESS ALERT. DOES NOT UNTAG THE ENTRIES POINTER. NOT GOOD, VERY BAD. DO NOT USE.
		// FIXME: WRONGNESS ALERT. DOES NOT UNTAG THE ENTRIES POINTER. NOT GOOD, VERY BAD. DO NOT USE.
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
