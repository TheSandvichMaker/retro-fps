#ifndef BUILD_H
#define BUILD_H

#include "core/core.h"

typedef enum optimization_level_t
{
    O0, O1, O2,
} optimization_level_t;

typedef enum warning_level_t
{
    W0, W1, W2, W3, W4,
} warning_level_t;

typedef enum warning_t
{
    WARNING_NONSTANDARD_EXT_ANONYMOUS_STRUCT,
	WARNING_NONSTANDARD_EXT_EMPTY_TRANSLATION_UNIT,
    WARNING_TYPE_DEFINITION_IN_PARENTHESIS,
    WARNING_FLEXIBLE_ARRAY_MEMBER, // why the heck is MSVC warning on this, this is part of C99
    WARNING_COUNT,
} warning_t;

typedef enum warning_state_t
{
    WARNING_STATE_SANE_DEFAULT,
    WARNING_STATE_DISABLED,
    WARNING_STATE_ENABLED,
} warning_state_t;

static warning_state_t warning_defaults[WARNING_COUNT] = {
    [WARNING_NONSTANDARD_EXT_ANONYMOUS_STRUCT]       = WARNING_STATE_DISABLED,
	[WARNING_NONSTANDARD_EXT_EMPTY_TRANSLATION_UNIT] = WARNING_STATE_DISABLED,
    [WARNING_TYPE_DEFINITION_IN_PARENTHESIS]         = WARNING_STATE_DISABLED,
    [WARNING_FLEXIBLE_ARRAY_MEMBER]                  = WARNING_STATE_DISABLED,
};

typedef enum backend_compiler_t
{
    BACKEND_MSVC,
    BACKEND_CLANG,
} backend_compiler_t;

typedef struct build_params_t
{
    bool no_cache;
    bool no_std_lib;

    backend_compiler_t backend;
    string_t configuration;
} build_params_t;

typedef struct compile_params_t
{
    bool     single_translation_unit;
	string_t stub_name;

    bool address_sanitizer;

    bool warnings_are_errors;
    warning_level_t warning_level;
    warning_state_t warnings[WARNING_COUNT];

    optimization_level_t optimization_level;

    string_list_t defines;
    string_list_t forced_includes;
    string_list_t include_paths;
    string_list_t ignored_files;
    string_list_t ignored_directories;
    string_t      additional_flags;
} compile_params_t;

typedef enum link_artefact_kind_t
{
	LINK_ARTEFACT_EXECUTABLE,
	LINK_ARTEFACT_DYNAMIC_LIBRARY,
	LINK_ARTEFACT_COUNT,
} link_artefact_kind_t;

typedef struct link_params_t
{
	link_artefact_kind_t kind;

    string_t output;
	string_t run_dir;
	bool copy_executables_to_run;

    string_list_t libraries;
    string_t      additional_flags;
} link_params_t;

typedef struct all_params_t
{
	build_params_t   build;
	compile_params_t compile;
	link_params_t    link;
} all_params_t;

typedef enum source_file_flags_t
{
    SOURCE_FILE_GENERATED = 1 << 0,
} source_file_flags_t;

typedef struct source_file_t
{
    struct source_file_t *next;
    struct source_file_t *prev;

    int flags;

    string_t name;
    string_t path;

    uint64_t last_write_time;

    // incremental compilation
    bool cached; 
} source_file_t;

typedef struct source_files_t
{
    source_file_t *first;
    source_file_t *last;
} source_files_t;

typedef enum build_error_t
{
    BUILD_ERROR_NONE,
    BUILD_ERROR_OTHER_FAILURE,
    BUILD_ERROR_COMPILATION,
    BUILD_ERROR_LINKER,
} build_error_t;

typedef struct build_context_t
{
    arena_t *arena;

    const build_params_t *params;

    string_t build_dir;
} build_context_t;

typedef struct object_t
{
	struct object_t *next;
	struct object_t *prev;
	string_t name;
} object_t;

typedef struct object_collection_t
{
	object_t *first;
	object_t *last;
} object_collection_t;

typedef enum compile_error_t
{
	COMPILE_ERROR_NONE,
	COMPILE_ERROR_OTHER_FAILURE,
} compile_error_t;

typedef enum link_error_t
{
	LINK_ERROR_NONE,
	LINK_ERROR_OTHER_FAILURE,
} link_error_t;

void gather_source_files(arena_t *arena, string_t directory, const compile_params_t *compile, source_files_t *source);
source_file_t *generate_file(build_context_t *context, string_t path, string_t contents);
void transform_into_stub(build_context_t *context, source_files_t *files, string_t stub_name);

compile_error_t compile_files    (build_context_t *context, source_files_t files, const build_params_t *build, const compile_params_t *compile, object_collection_t *objects);
compile_error_t compile_directory(build_context_t *context, string_t directory,   const build_params_t *build, const compile_params_t *compile, object_collection_t *objects);

link_error_t link_executable(build_context_t *context, const object_collection_t *objects, const build_params_t *build, const link_params_t *link);

build_error_t build_files    (build_context_t *context, source_files_t files, const all_params_t *params);
build_error_t build_directory(build_context_t *context, string_t directory,   const all_params_t *params);

#endif /* BUILD_H */
