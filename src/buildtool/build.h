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
    WARNING_ANONYMOUS_STRUCT,
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
    [WARNING_ANONYMOUS_STRUCT]               = WARNING_STATE_DISABLED,
    [WARNING_TYPE_DEFINITION_IN_PARENTHESIS] = WARNING_STATE_DISABLED,
    [WARNING_FLEXIBLE_ARRAY_MEMBER]          = WARNING_STATE_DISABLED,
};

typedef enum backend_compiler_t
{
    BACKEND_MSVC,
    BACKEND_CLANG,
} backend_compiler_t;

typedef struct build_job_t
{
    bool no_cache;
    bool no_std_lib;
    bool single_translation_unit;
    bool address_sanitizer;

    backend_compiler_t backend;

    bool warnings_are_errors;
    warning_level_t warning_level;
    warning_state_t warnings[WARNING_COUNT];

    optimization_level_t optimization_level;

    string_t output_exe;
    string_t configuration;

    string_list_t defines;
    string_list_t forced_includes;
    string_list_t include_paths;
    string_list_t ignored_files;
    string_list_t ignored_directories;
    string_list_t libraries;
    string_t      additional_flags;
} build_job_t;

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

typedef enum build_result_t
{
    BUILD_SUCCESS,
    BUILD_OTHER_FAILURE,
    BUILD_COMPILATION_ERROR,
    BUILD_LINKER_ERROR,
} build_result_t;

typedef struct build_context_t
{
    arena_t *arena;

    build_job_t *job;

    string_t build_dir;
} build_context_t;

void gather_source_files(arena_t *arena, string_t directory, build_job_t *job, source_files_t *source);
source_file_t *generate_file(build_context_t *context, string_t path, string_t contents);
build_result_t build_files(source_files_t files, build_job_t *job);
build_result_t build_directory(string_t directory, build_job_t *job);

#endif /* BUILD_H */
