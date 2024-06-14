#pragma once

typedef struct df_shader_info_t
{
	bool loaded;

	string_t name;
	string_t path_dfs;
	string_t path_hlsl;

	rhi_shader_bytecode_t bytecode;
} df_shader_info_t;
