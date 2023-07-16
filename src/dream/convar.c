#include "convar.h"
#include "core/common.h"
#include "core/string.h"
#include "core/bulk_data.h"
#include "core/hashtable.h"

static bulk_t g_convars = INIT_BULK_DATA(convar_t);
static hash_t g_convar_from_key;

void con_register(string_t key, const convar_t *var)
{
    ASSERT(key.count <= ARRAY_COUNT(var->key.data));

    resource_handle_t handle = bd_add_item(&g_convars, var);

    convar_t *result = bd_get(&g_convars, handle);
    STRING_INTO_STORAGE(result->key, key);

    hash_add(&g_convar_from_key, string_hash(key), handle.value);
}

convar_t *con_find(string_t key)
{
    convar_t *result = NULL;

    resource_handle_t handle;
    if (hash_find(&g_convar_from_key, string_hash(key), &handle.value))
    {
        result = bd_get(&g_convars, handle);
        ASSERT(string_match(STRING_FROM_STORAGE(result->key), key));
    }

    return result;
}
