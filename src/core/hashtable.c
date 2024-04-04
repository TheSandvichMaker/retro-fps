#include "hashtable.h"
#include "common.h"
#include "assert.h"
#include "vm.h"

static void table_init(table_t *table)
{
    ASSERT(!table->entries);

    table->entries = vm_reserve(NULL, sizeof(table_entry_t)*TABLE_RESERVE_CAPACITY);
    vm_commit(table->entries, sizeof(table_entry_t)*256); // one page worth of entries

    table->mask = 255;
    table->load = 0;
}

static void table_grow(table_t *table)
{
    table_entry_t *new_entries, *old_entries;

    if (TABLE_POINTER_HAS_TAGS(table->entries, TABLE_SECONDARY_BUFFER))
        new_entries = (table_entry_t *)TABLE_UNTAG_POINTER(table->entries) - TABLE_HALF_CAPACITY;
    else
        new_entries = (table_entry_t *)TABLE_UNTAG_POINTER(table->entries) + TABLE_HALF_CAPACITY;

    old_entries = TABLE_UNTAG_POINTER(table->entries);

    uint32_t old_capacity = table->mask + 1;
    uint32_t new_capacity = old_capacity*2;

    vm_commit(new_entries, sizeof(table_entry_t)*new_capacity);
    zero_array(new_entries, old_capacity);

    if (new_entries > old_entries)
        new_entries = TABLE_TAG_POINTER(new_entries, TABLE_SECONDARY_BUFFER);

    table_t new_table = {
        .mask    = new_capacity - 1,
        .entries = new_entries,
    };

    for (size_t i = 0; i < old_capacity; i++)
    {
        table_entry_t *old_entry = &old_entries[i];

        if (old_entry->key != TABLE_UNUSED_KEY_VALUE)
            table_insert(&new_table, old_entry->key, old_entry->value);
    }

    copy_struct(table, &new_table);
}

static bool table_find_slot(const table_t *table, uint64_t key, uint64_t *slot)
{
    if (!table->entries)
        return false;

    bool result = false;

    uint64_t       mask    = table->mask;
    table_entry_t *entries = TABLE_UNTAG_POINTER(table->entries);

    if (key == TABLE_UNUSED_KEY_VALUE)
        key = 1;

    uint64_t probe = key & mask;
    for (;;)
    {
        table_entry_t *entry = &entries[probe];

        if (entry->key == TABLE_UNUSED_KEY_VALUE)
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

bool table_find(const table_t *table, uint64_t key, uint64_t *value)
{
	bool result = false;

	uint64_t slot;
	if (table_find_slot(table, key, &slot))
	{
		table_entry_t *entries = TABLE_UNTAG_POINTER(table->entries);

		*value = entries[slot].value;
		result = true;
	}

	return result;
}

void table_insert(table_t *table, uint64_t key, uint64_t value)
{
    if (!table->entries)
        table_init(table);

    if (table->load*4 >= (table->mask + 1)*3)
        table_grow(table);

    uint64_t       mask    = table->mask;
    table_entry_t *entries = TABLE_UNTAG_POINTER(table->entries);

    if (key == TABLE_UNUSED_KEY_VALUE)
        key = 1;

    uint64_t probe = key & mask;
    for (;;)
    {
        table_entry_t *entry = &entries[probe];

        if (entry->key == TABLE_UNUSED_KEY_VALUE ||
            entry->key == key)
        {
            table->load += (entry->key == TABLE_UNUSED_KEY_VALUE);

            entry->key   = key;
            entry->value = value;

            break;
        }

        probe = (probe + 1) & mask;

        if (NEVER(probe == key))
            break; // table is full! that shouldn't happen!
    }
}

bool table_remove(table_t *table, uint64_t key)
{
	uint64_t i;

	if (!table_find_slot(table, key, &i))
		return false;

    uint64_t       mask    = table->mask;
    table_entry_t *entries = TABLE_UNTAG_POINTER(table->entries);

	if (entries[i].key == TABLE_UNUSED_KEY_VALUE)
		return false;

	entries[i].key = TABLE_UNUSED_KEY_VALUE;

	uint64_t j = i;
	for (;;)
	{
		j = (j + 1) & mask;

		if (entries[j].key == TABLE_UNUSED_KEY_VALUE)
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
		entries[j].key = TABLE_UNUSED_KEY_VALUE;
		i = j;
	}

	return true;
}

void table_release(table_t *table)
{
    table->mask = 0;
    table->load = 0;
    vm_release(table->entries);
    table->entries = NULL;
}

void *table_find_object(const table_t *table, uint64_t key)
{
	void *result = NULL;
    table_find(table, key, (uint64_t *)&result);
	return result;
}

void table_insert_object(table_t *table, uint64_t key, void *value)
{
    table_insert(table, key, (uint64_t)value);
}
