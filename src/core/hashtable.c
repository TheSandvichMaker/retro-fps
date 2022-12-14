#include "hashtable.h"
#include "common.h"
#include "assert.h"
#include "vm.h"

// TODO: removal (chain repair)

enum { HASHTABLE_RESERVE_CAPACITY = 1 << 20, HASHTABLE_HALF_CAPACITY = 1 << 19 }; // up to ~500k entries
enum { HASH_SECONDARY_BUFFER = 1 << 0, HASH_EXTERNAL_MEMORY = 1 << 1 };

#define   TAG_POINTER(pointer, tags) (void *)((uintptr_t)(pointer)|(tags))
#define UNTAG_POINTER(pointer)       (void *)((uintptr_t)(pointer) & ~(alignof(hash_entry_t) - 1))
#define POINTER_HAS_TAGS(pointer, tags) !!((uintptr_t)(pointer) & tags)

static void hash_init(hash_t *hash)
{
    ASSERT(!hash->entries);

    hash->entries = vm_reserve(NULL, sizeof(hash_entry_t)*HASHTABLE_RESERVE_CAPACITY);
    vm_commit(hash->entries, sizeof(hash_entry_t)*256); // one page worth of entries

    hash->mask = 255;
    hash->load = 0;
}

static void hash_grow(hash_t *hash)
{
    hash_entry_t *new_entries, *old_entries;

    if (POINTER_HAS_TAGS(hash->entries, HASH_SECONDARY_BUFFER))
        new_entries = (hash_entry_t *)UNTAG_POINTER(hash->entries) - HASHTABLE_HALF_CAPACITY;
    else
        new_entries = (hash_entry_t *)UNTAG_POINTER(hash->entries) + HASHTABLE_HALF_CAPACITY;

    old_entries = UNTAG_POINTER(hash->entries);

    uint32_t old_capacity = hash->mask + 1;
    uint32_t new_capacity = old_capacity*2;

    vm_commit(new_entries, sizeof(hash_entry_t)*new_capacity);
    zero_array(new_entries, old_capacity);

    if (new_entries > old_entries)
        new_entries = TAG_POINTER(new_entries, HASH_SECONDARY_BUFFER);

    hash_t new_hash = {
        .mask    = new_capacity - 1,
        .entries = new_entries,
    };

    for (size_t i = 0; i < old_capacity; i++)
    {
        hash_entry_t *old_entry = &old_entries[i];

        if (old_entry->key != UNUSED_KEY_VALUE)
            hash_add(&new_hash, old_entry->key, old_entry->value);
    }

    copy_struct(hash, &new_hash);
}

bool hash_find(const hash_t *hash, uint64_t key, uint64_t *value)
{
    if (!hash->entries)
        return false;

    bool result = false;

    uint64_t      mask    = hash->mask;
    hash_entry_t *entries = UNTAG_POINTER(hash->entries);

    if (key == UNUSED_KEY_VALUE)
        key = 1;

    uint64_t probe = key & mask;
    for (;;)
    {
        hash_entry_t *entry = &entries[probe];

        if (entry->key == UNUSED_KEY_VALUE)
            break;

        if (entry->key == key)
        {
            *value = entry->value;
            result = true;
            break;
        }

        probe = (probe + 1) & mask;
    }

    return result;
}

void hash_add(hash_t *hash, uint64_t key, uint64_t value)
{
    if (!hash->entries)
        hash_init(hash);

    if (hash->load*4 >= (hash->mask + 1)*3)
        hash_grow(hash);

    uint64_t      mask    = hash->mask;
    hash_entry_t *entries = UNTAG_POINTER(hash->entries);

    if (key == UNUSED_KEY_VALUE)
        key = 1;

    uint64_t probe = key & mask;
    for (;;)
    {
        hash_entry_t *entry = &entries[probe];

        if (entry->key == UNUSED_KEY_VALUE ||
            entry->key == key)
        {
            entry->key   = key;
            entry->value = value;
            hash->load += (entry->key == UNUSED_KEY_VALUE);
            break;
        }

        probe = (probe + 1) & mask;

        if (NEVER(probe == key))
            break;
    }
}

bool hash_find_ptr(const hash_t *table, uint64_t key, void **value)
{
    return hash_find(table, key, (uint64_t *)value);
}

void hash_add_ptr(hash_t *table, uint64_t key, void *value)
{
    hash_add(table, key, (uint64_t)value);
}
