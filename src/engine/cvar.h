#pragma once

typedef enum cvar_kind_t
{
	CVarKind_NONE,

	CVarKind_bool,
	CVarKind_i32,
	CVarKind_f32,
	CVarKind_string,

	CVarKind_COUNT,
} cvar_kind_t;

typedef enum cvar_flags_t
{
	CVarFlag_registered = 0x1,
} cvar_flags_t;

typedef union cvar_value_t
{
	bool     boolean;
	int32_t  i32;
	float    f32;
	string_t string;
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

	cvar_value_t default_value;
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

typedef struct cvar_state_t
{
	bool initialized;

	table_t cvar_table;

	arena_t       arena;
	simple_heap_t string_allocator;
} cvar_state_t;

global cvar_state_t g_cvars;

#define CVAR_BOOL(in_variable, in_key, in_default_value)                      \
	global cvar_t in_variable = {                                             \
		.kind       = CVarKind_bool,                                          \
		.key        = Sc(in_key),                                             \
		.as.boolean = !!(in_default_value),                                   \
	};

#define CVAR_I32(in_variable, in_key, in_default_value)                       \
	global cvar_t in_variable = {                                             \
		.kind    = CVarKind_i32,                                              \
		.key     = Sc(in_key),                                                \
		.as.i32  = in_default_value,                                          \
		.min.i32 = INT32_MIN,                                                 \
		.max.i32 = INT32_MAX,                                                 \
	};

#define CVAR_F32(in_variable, in_key, in_default_value)                       \
	global cvar_t in_variable = {                                             \
		.kind    = CVarKind_f32,                                              \
		.key     = Sc(in_key),                                                \
		.as.f32  = in_default_value,                                          \
		.min.f32 = -INFINITY,                                                 \
		.max.f32 =  INFINITY,                                                 \
	};

#define CVAR_I32_EX(in_variable, in_key, in_default_value, in_min, in_max)    \
	global cvar_t in_variable = {                                             \
		.kind    = CVarKind_i32,                                              \
		.key     = Sc(in_key),                                                \
		.as.i32  = in_default_value,                                          \
		.min.i32 = in_min,                                                    \
		.max.i32 = in_max,                                                    \
	};

#define CVAR_F32_EX(in_variable, in_key, in_default_value, in_min, in_max)    \
	global cvar_t in_variable = {                                             \
		.kind    = CVarKind_f32,                                              \
		.key     = Sc(in_key),                                                \
		.as.f32  = in_default_value,                                          \
		.min.f32 = in_min,                                                    \
		.max.f32 = in_max,                                                    \
	};

#define CVAR_STRING(in_variable, in_key, in_default_value)                    \
	global cvar_t in_variable = {                                             \
		.kind      = CVarKind_string,                                         \
		.key       = Sc(in_key),                                              \
		.as.string = Sc(in_default_value),                                    \
	};

fn void cvar_init_system(void);

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

fn table_iter_t cvar_iter(void);
