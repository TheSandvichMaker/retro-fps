#pragma once

typedef struct df_shader_info_t
{
	bool loaded;

	string_t name;
	string_t entry_point;
	string_t target;

	string_t hlsl_source;

	string_t path_hlsl;
	string_t path_dfs;

	rhi_shader_bytecode_t bytecode;
} df_shader_info_t;

typedef struct df_pso_info_t
{
	string_t name;
	string_t path;

	uint32_t vs_index; 
	uint32_t ps_index;

	rhi_create_graphics_pso_params_t params_without_bytecode;
} df_pso_info_t;