#pragma once

typedef struct df_shader_info_t
{
	bool loaded;

	string_t name;
	string_t entry_point;
	string_t target;

	string_t path_hlsl;
	string_t path_dfs;

	rhi_shader_bytecode_t bytecode;
} df_shader_info_t;
