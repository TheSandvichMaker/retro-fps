void cvar_register(cvar_t *cvar)
{
	if (ALWAYS(!(cvar->flags & CVarFlag_registered)))
	{
		m_scoped_temp
		{
			string_t key_lower = string_to_lower(temp, cvar->key);
			table_insert_object(&g_cvars.cvar_table, string_hash(key_lower), cvar);
		}

		cvar->flags |= CVarFlag_registered;
	}
}

cvar_t *cvar_find(string_t key)
{
	cvar_t *result = NULL;

	m_scoped_temp
	{
		string_t key_lower = string_to_lower(temp, key);
		result = table_find_object(&g_cvars.cvar_table, string_hash(key_lower));

		DEBUG_ASSERT(string_match_nocase(result->key, key_lower));
	}

	return result;
}

null_term_string_t cvar_kind_to_string(cvar_kind_t kind)
{
	string_t result = {0};

	switch (kind)
	{
		case CVarKind_NONE:   result = S("<none>"); break;
		case CVarKind_bool:   result = S("bool");   break;
		case CVarKind_i32:    result = S("i32");    break;
		case CVarKind_f32:    result = S("f32");    break;
		case CVarKind_string: result = S("string"); break;
		INVALID_DEFAULT_CASE;
	}

	return result;
}

null_term_string_t cvar_kind_to_string_with_indefinite_article(cvar_kind_t kind)
{
	string_t result = {0};

	switch (kind)
	{
		case CVarKind_NONE:   result = S("<none>");   break;
		case CVarKind_bool:   result = S("a bool");   break;
		case CVarKind_i32:    result = S("an i32");   break;
		case CVarKind_f32:    result = S("an f32");   break;
		case CVarKind_string: result = S("a string"); break;
		INVALID_DEFAULT_CASE;
	}

	return result;
}

bool cvar_read_bool(cvar_t *cvar)
{
	ASSERT_MSG(cvar->kind == CVarKind_bool, "Tried to read cvar %.*s as a bool but it's %s", 
			   Sx(cvar->key),
			   cvar_kind_to_string_with_indefinite_article(cvar->kind).data);

	if (!(cvar->flags & CVarFlag_registered))
	{
		cvar_register(cvar);
	}
	
	return cvar->as.boolean;
}

int32_t cvar_read_i32(cvar_t *cvar)
{
	ASSERT_MSG(cvar->kind == CVarKind_i32, "Tried to read cvar %.*s as an i32 but it's a %.*s", 
			   Sx(cvar->key),
			   Sx(cvar_kind_to_string(cvar->kind)));

	if (!(cvar->flags & CVarFlag_registered))
	{
		cvar_register(cvar);
	}
	
	return cvar->as.i32;
}

float cvar_read_f32(cvar_t *cvar)
{
	ASSERT_MSG(cvar->kind == CVarKind_f32, "Tried to read cvar %.*s as an f32 but it's a %.*s", 
			   Sx(cvar->key),
			   Sx(cvar_kind_to_string(cvar->kind)));

	if (!(cvar->flags & CVarFlag_registered))
	{
		cvar_register(cvar);
	}
	
	return cvar->as.f32;
}

string_t cvar_read_string(cvar_t *cvar)
{
	ASSERT_MSG(cvar->kind == CVarKind_string, "Tried to read cvar %.*s as a string but it's a %.*s", 
			   Sx(cvar->key),
			   Sx(cvar_kind_to_string(cvar->kind)));

	if (!(cvar->flags & CVarFlag_registered))
	{
		cvar_register(cvar);
	}
	
	return cvar->as.string;
}
