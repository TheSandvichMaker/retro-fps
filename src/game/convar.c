// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

global pool_t g_convars = INIT_POOL(convar_t);
global table_t g_convar_from_key;

void con_register(string_t key, const convar_t *var)
{
    ASSERT(key.count <= ARRAY_COUNT(var->key.data));

    resource_handle_t handle = pool_add_item(&g_convars, var);

    convar_t *result = pool_get(&g_convars, handle);
    string_into_storage(result->key, key);

    table_insert(&g_convar_from_key, string_hash(key), handle.value);
}

convar_t *con_find(string_t key)
{
    convar_t *result = NULL;

    resource_handle_t handle;
    if (table_find(&g_convar_from_key, string_hash(key), &handle.value))
    {
        result = pool_get(&g_convars, handle);
        ASSERT(string_match(string_from_storage(result->key), key));
    }

    return result;
}
