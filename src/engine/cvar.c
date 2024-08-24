void cvar_init_system(void)
{
	ASSERT(!g_cvars.initialized);

	simple_heap_init(&g_cvars.string_allocator, &g_cvars.arena);

	g_cvars.initialized = true;
}

void cvar_register(cvar_t *cvar)
{
	ASSERT(g_cvars.initialized);

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
	ASSERT(g_cvars.initialized);

	cvar_t *result = NULL;

	m_scoped_temp
	{
		string_t key_lower = string_to_lower(temp, key);
		result = table_find_object(&g_cvars.cvar_table, string_hash(key_lower));

		DEBUG_ASSERT(string_match_nocase(result->key, key_lower));
	}

	return result;
}

size_t get_cvar_count(void)
{
	return g_cvars.cvar_table.load;
}

null_term_string_t cvar_kind_to_string(cvar_kind_t kind)
{
	string_t result = {0};

	switch (kind)
	{
		case CVarKind_none:    result = S("<none>");  break;
		case CVarKind_bool:    result = S("bool");    break;
		case CVarKind_i32:     result = S("i32");     break;
		case CVarKind_f32:     result = S("f32");     break;
		case CVarKind_string:  result = S("string");  break;
		case CVarKind_command: result = S("command"); break;
		INVALID_DEFAULT_CASE;
	}

	return result;
}

null_term_string_t cvar_kind_to_string_with_indefinite_article(cvar_kind_t kind)
{
	string_t result = {0};

	switch (kind)
	{
		case CVarKind_none:    result = S("<none>");    break;
		case CVarKind_bool:    result = S("a bool");    break;
		case CVarKind_i32:     result = S("an i32");    break;
		case CVarKind_f32:     result = S("an f32");    break;
		case CVarKind_string:  result = S("a string");  break;
		case CVarKind_command: result = S("a command"); break;
		INVALID_DEFAULT_CASE;
	}

	return result;
}

fn_local bool cvar_validate_and_register(cvar_t *cvar, cvar_kind_t expected_kind)
{
	bool initialized = g_cvars.initialized;

	if (!initialized)
	{
		log(CVar, Error, "CVar subsystem is not initialized!");
	}

	bool valid = (cvar->kind == expected_kind);

	if (!valid)
	{
		log(CVar, Error, 
			"Tried to read cvar %cs as %cs but it's %cs", 
			cvar->key,
			cvar_kind_to_string_with_indefinite_article(expected_kind),
			cvar_kind_to_string_with_indefinite_article(cvar->kind));
	}

	bool result = initialized & valid;

	if (!(cvar->flags & CVarFlag_registered))
	{
		log(CVar, Warning, "Lazily registered cvar '%cs'", cvar->key);
		cvar_register(cvar);
	}

	return result;
}

bool cvar_read_bool(cvar_t *cvar)
{
	if (cvar_validate_and_register(cvar, CVarKind_bool)) 
	{
		return cvar->as.boolean; 
	}
	return cvar->as_default.boolean;
}

int32_t cvar_read_i32(cvar_t *cvar)
{
	if (cvar_validate_and_register(cvar, CVarKind_i32))
	{
		return cvar->as.i32;
	}
	return cvar->as_default.i32;
}

float cvar_read_f32(cvar_t *cvar)
{
	if (cvar_validate_and_register(cvar, CVarKind_f32))
	{
		return cvar->as.f32;
	}
	return cvar->as_default.f32;
}

string_t cvar_read_string(cvar_t *cvar)
{
	if (cvar_validate_and_register(cvar, CVarKind_string))
	{
		return cvar->as.string;
	}
	return cvar->as_default.string;
}

void cvar_write_bool(cvar_t *cvar, bool value)
{
	if (cvar_validate_and_register(cvar, CVarKind_bool))
	{
		cvar->as.boolean = value;
	}
}

void cvar_write_i32(cvar_t *cvar, int32_t value)
{
	if (cvar_validate_and_register(cvar, CVarKind_i32))
	{
		cvar->as.i32 = value;
	}
}

void cvar_write_f32(cvar_t *cvar, float value)
{
	if (cvar_validate_and_register(cvar, CVarKind_f32))
	{
		cvar->as.f32 = value;
	}
}

void cvar_write_string(cvar_t *cvar, string_t string)
{
	if (cvar_validate_and_register(cvar, CVarKind_string))
	{
		cvar_string_block_t *old_block = cvar->string_block;

		uint16_t string_block_header_size = offsetof(cvar_string_block_t, bytes);

		if (NEVER(string.count > UINT16_MAX - string_block_header_size))
		{
			string.count = UINT16_MAX - string_block_header_size;
		}

		uint16_t block_size = (uint16_t)(string_block_header_size + string.count);
		cvar->string_block       = simple_heap_alloc(&g_cvars.string_allocator, block_size);
		cvar->string_block->size = block_size;

		memcpy(cvar->string_block->bytes, string.data, string.count);

		cvar->as.string = (string_t){ .data = cvar->string_block->bytes, .count = string.count };

		if (old_block)
		{
			simple_heap_free(&g_cvars.string_allocator, old_block, old_block->size);
		}
	}
}

void cvar_execute_command(cvar_t *cvar, string_t arguments)
{
	if (cvar_validate_and_register(cvar, CVarKind_command))
	{
		if (cvar->as.command)
		{
			cvar->as.command(arguments);
		}
		else
		{
			log(CVar, Error, "Cvar '%cs' has no command bound to it!", cvar->key);
		}
	}
}

bool cvar_is_default(cvar_t *cvar)
{
	bool result = false;

	switch (cvar->kind)
	{
		case CVarKind_bool:
		{
			result = cvar->as.boolean == cvar->as_default.boolean;
		} break;

		case CVarKind_i32:
		{
			result = cvar->as.i32 == cvar->as_default.i32;
		} break;

		case CVarKind_f32:
		{
			result = cvar->as.f32 == cvar->as_default.f32;
		} break;

		case CVarKind_string:
		{
			result = string_match(cvar->as.string, cvar->as_default.string);
		} break;

		case CVarKind_command:
		{
			result = cvar->as.command == cvar->as_default.command;
		} break;
	}

	return result;
}

void cvar_reset_to_default(cvar_t *cvar)
{
	switch (cvar->kind)
	{
		case CVarKind_bool:
		{
			cvar->as.boolean = cvar->as_default.boolean;
		} break;

		case CVarKind_i32:
		{
			cvar->as.i32 = cvar->as_default.i32;
		} break;

		case CVarKind_f32:
		{
			cvar->as.f32 = cvar->as_default.f32;
		} break;

		case CVarKind_string:
		{
			cvar->as.string = cvar->as_default.string;

			if (cvar->string_block)
			{
				simple_heap_free(&g_cvars.string_allocator, cvar->string_block, cvar->string_block->size);
				cvar->string_block = NULL;
			}
		} break;
	}
}

table_iter_t cvar_iter(void)
{
	return table_iter(&g_cvars.cvar_table);
}
