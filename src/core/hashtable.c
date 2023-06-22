#include "hashtable.h"
#include "common.h"
#include "assert.h"
#include "vm.h"

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

static bool hash_find_slot(const hash_t *hash, uint64_t key, uint64_t *slot)
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
			*slot = probe;
            result = true;
            break;
        }

        probe = (probe + 1) & mask;
    }

    return result;
}

bool hash_find(const hash_t *hash, uint64_t key, uint64_t *value)
{
	bool result = false;

	uint64_t slot;
	if (hash_find_slot(hash, key, &slot))
	{
		hash_entry_t *entries = UNTAG_POINTER(hash->entries);

		*value = entries[slot].value;
		result = true;
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
            hash->load += (entry->key == UNUSED_KEY_VALUE);

            entry->key   = key;
            entry->value = value;

            break;
        }

        probe = (probe + 1) & mask;

        if (NEVER(probe == key))
            break;
    }
}

bool hash_rem(hash_t *hash, uint64_t key)
{
	uint64_t i;

	if (!hash_find_slot(hash, key, &i))
		return false;

    uint64_t      mask    = hash->mask;
    hash_entry_t *entries = UNTAG_POINTER(hash->entries);

	if (entries[i].key == UNUSED_KEY_VALUE)
		return false;

	entries[i].key = UNUSED_KEY_VALUE;

	uint64_t j = i;
	for (;;)
	{
		j = (j + 1) & mask;

		if (entries[j].key == UNUSED_KEY_VALUE)
			break;

		uint64_t k = entries[j].key & mask;

		if (i <= j)
		{
			if (i < k && k <= j)
				continue;
		}
		else
		{
			if (i < k || k <= j)
				continue;
		}

		entries[i] = entries[j];
		entries[j].key = UNUSED_KEY_VALUE;
		i = j;
	}

	return true;
}

void *hash_find_object(const hash_t *table, uint64_t key)
{
	void *result = NULL;
    hash_find(table, key, (uint64_t *)&result);
	return result;
}

void hash_add_object(hash_t *table, uint64_t key, void *value)
{
    hash_add(table, key, (uint64_t)value);
}
