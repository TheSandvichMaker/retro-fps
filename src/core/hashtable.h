#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "api_types.h"

enum { HASHTABLE_UNUSED_KEY_VALUE = 0 };

enum { HASHTABLE_RESERVE_CAPACITY = 1 << 20, HASHTABLE_HALF_CAPACITY = 1 << 19 }; // up to ~500k entries
enum { HASHTABLE_SECONDARY_BUFFER = 1 << 0, HASHTABLE_EXTERNAL_MEMORY = 1 << 1 };

#define   HT_TAG_POINTER(pointer, tags) (void *)((uintptr_t)(pointer)|(tags))
#define HT_UNTAG_POINTER(pointer)       (void *)((uintptr_t)(pointer) & ~(alignof(hash_entry_t) - 1))
#define HT_POINTER_HAS_TAGS(pointer, tags) !!((uintptr_t)(pointer) & tags)

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

DREAM_GLOBAL bool hash_find(const hash_t *table, uint64_t key, uint64_t *value);
DREAM_GLOBAL void hash_add (      hash_t *table, uint64_t key, uint64_t  value);
DREAM_GLOBAL bool hash_rem (      hash_t *table, uint64_t key);

DREAM_GLOBAL void *hash_find_object(const hash_t *table, uint64_t key);
DREAM_GLOBAL void  hash_add_object (      hash_t *table, uint64_t key, void *value);

DREAM_INLINE hash_entry_t *hash_get_entries(const hash_t *table)
{
	return HT_UNTAG_POINTER(table->entries);
}

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

	hash_entry_t *entries = hash_get_entries(table);

    while (it->index <= table->mask)
    {
        if (entries[it->index].key != HASHTABLE_UNUSED_KEY_VALUE)
        {
            result = true;

            it->value = entries[it->index].value;
            it->index += 1;

            break;
        }

        it->index += 1;
    }

    return result;
}

#endif /* HASHTABLE_H */
