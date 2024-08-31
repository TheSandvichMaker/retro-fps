#pragma once

typedef void (*cvar_command_t)(string_t arguments);

typedef enum cvar_kind_t
{
	CVarKind_none,

	CVarKind_bool,
	CVarKind_i32,
	CVarKind_f32,
	CVarKind_string,
	CVarKind_command,

	CVarKind_COUNT,
} cvar_kind_t;

typedef enum cvar_flags_t
{
	CVarFlag_registered = 0x1,
} cvar_flags_t;

typedef union cvar_value_t
{
	bool           boolean;
	int32_t        i32;
	float          f32;
	string_t       string;
	cvar_command_t command;
} cvar_value_t;

typedef struct cvar_string_block_t
{
	uint16_t size;
	char     bytes[1];
} cvar_string_block_t;

typedef struct cvar_t
{
	cvar_kind_t  kind;
	cvar_flags_t flags;

	string_t key;

	cvar_value_t as_default;
	cvar_value_t as;

	cvar_string_block_t *string_block;

	union
	{
		int32_t i32;
		float   f32;
	} min;

	union
	{
		int32_t i32;
		float   f32;
	} max;
} cvar_t;

typedef struct cmd_execution_node_t
{
	struct cmd_execution_node_t *next;
	cvar_t  *cmd;
	string_t arguments;
} cmd_execution_node_t;

typedef struct cmd_execution_list_t
{
	cmd_execution_node_t *head;
	cmd_execution_node_t *tail;
} cmd_execution_list_t;

typedef struct cvar_state_t
{
	bool initialized;

	table_t cvar_table;

	arena_t       arena;
	simple_heap_t string_allocator;
} cvar_state_t;

// global cvar_state_t g_cvars;
thread_local cvar_state_t *g_cvars;

fn void equip_cvar_state(cvar_state_t *cvar_state);
fn void unequip_cvar_state(void);

#define CVAR_BOOL(in_variable, in_key, in_default_value)                      \
	global cvar_t in_variable = {                                             \
		.kind               = CVarKind_bool,                                  \
		.key                = Sc(in_key),                                     \
		.as.boolean         = !!(in_default_value),                           \
		.as_default.boolean = !!(in_default_value),                           \
	};

#define CVAR_I32(in_variable, in_key, in_default_value)                       \
	global cvar_t in_variable = {                                             \
		.kind            = CVarKind_i32,                                      \
		.key             = Sc(in_key),                                        \
		.as.i32          = in_default_value,                                  \
		.as_default.i32  = in_default_value,                                  \
	};

#define CVAR_F32(in_variable, in_key, in_default_value)                       \
	global cvar_t in_variable = {                                             \
		.kind           = CVarKind_f32,                                       \
		.key            = Sc(in_key),                                         \
		.as.f32         = in_default_value,                                   \
		.as_default.f32 = in_default_value,                                   \
	};

#define CVAR_I32_EX(in_variable, in_key, in_default_value, in_min, in_max)    \
	global cvar_t in_variable = {                                             \
		.kind           = CVarKind_i32,                                       \
		.key            = Sc(in_key),                                         \
		.as.i32         = in_default_value,                                   \
		.as_default.i32 = in_default_value,                                   \
		.min.i32        = in_min,                                             \
		.max.i32        = in_max,                                             \
	};

#define CVAR_F32_EX(in_variable, in_key, in_default_value, in_min, in_max)    \
	global cvar_t in_variable = {                                             \
		.kind           = CVarKind_f32,                                       \
		.key            = Sc(in_key),                                         \
		.as.f32         = in_default_value,                                   \
		.as_default.f32 = in_default_value,                                   \
		.min.f32        = in_min,                                             \
		.max.f32        = in_max,                                             \
	};

#define CVAR_STRING(in_variable, in_key, in_default_value)                    \
	global cvar_t in_variable = {                                             \
		.kind              = CVarKind_string,                                 \
		.key               = Sc(in_key),                                      \
		.as.string         = Sc(in_default_value),                            \
		.as_default.string = Sc(in_default_value),                            \
	};

#define CVAR_COMMAND(in_variable, in_key)                        \
	fn_local void PASTE(in_variable, _func)(string_t arguments); \
																 \
	global cvar_t in_variable = {                                \
		.kind               = CVarKind_command,                  \
		.key                = Sc(in_key),                        \
		.as.command         = PASTE(in_variable, _func),         \
		.as_default.command = PASTE(in_variable, _func),         \
	};                                                           \
																 \
	void PASTE(in_variable, _func)(string_t arguments)

fn void cvar_state_init(cvar_state_t *cvar_state);

fn void    cvar_register (cvar_t *cvar);
fn cvar_t *cvar_find     (string_t key);
fn size_t  get_cvar_count(void);

fn bool     cvar_read_bool  (cvar_t *cvar);
fn int32_t  cvar_read_i32   (cvar_t *cvar);
fn float    cvar_read_f32   (cvar_t *cvar);
fn string_t cvar_read_string(cvar_t *cvar);

fn void cvar_write_bool  (cvar_t *cvar, bool     value);
fn void cvar_write_i32   (cvar_t *cvar, int32_t  value);
fn void cvar_write_f32   (cvar_t *cvar, float    value);
fn void cvar_write_string(cvar_t *cvar, string_t value);

fn void cvar_execute_command(cvar_t *cvar, string_t arguments);

fn bool cvar_is_default(cvar_t *cvar);
fn void cvar_reset_to_default(cvar_t *cvar);

fn table_iter_t cvar_iter(void);
