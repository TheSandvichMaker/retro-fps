#define UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build.h"
#include "assets.h"
#include "backend.h"

// unity build
arena_t static_arena;

// TODO: Move to somewhere nicer
static string_t format_seconds(arena_t *arena, double seconds)
{
    if (seconds < 1.0)
    {
        return string_format(arena, "%d milliseconds", (int)(1000.0*seconds));
    }
    else if (seconds < 60.0)
    {
        return string_format(arena, "%.02f seconds", seconds);
    }
    else if (seconds / 60.0 < 60.0)
    {
        return string_format(arena, "%d minutes and %d seconds", (int)(seconds / 60.0), (int)seconds % 60);
    }
    else
    {
        return string_format(arena, "%d hours, %d minutes and %d seconds", (int)(seconds / 60.0 / 24.0), (int)(seconds / 60.0), (int)seconds % 60);
    }
}

#include "core/arena.c"
#include "core/string.c"
#include "core/string_list.c"
#include "core/os.c"
#include "core/utility.c"
#include "core/tls.c"

#include "backend_msvc.c"
#include "backend_clang.c"
#include "build.c"
#include "assets.c"

typedef struct cmd_args_t
{
    char **at;
    char **end;

    bool error;
} cmd_args_t;

static void init_args(cmd_args_t *args, int argc, char **argv)
{
    args->error = false;

    args->at  = argv + 1;
    args->end = args->at + argc - 1;
}

static bool args_left(cmd_args_t *args)
{
    return args->at < args->end;
}

static bool args_match(cmd_args_t *args, const char *arg)
{
    if (strcmp(*args->at, arg) == 0)
    {
        args->at += 1;
        return true;
    }
    return false;
}

static void args_error(cmd_args_t *args, string_t failure_reason)
{
    fprintf(stderr, "argument parse error: %.*s", strexpand(failure_reason));
    args->error = true;
}

static string_t args_next(cmd_args_t *args)
{
    if (args_left(args))
    {
        string_t result = string_from_cstr(*args->at);
        args->at += 1;
        return result;
    }
    else
    {
        args_error(args, strlit("expected another argument"));
        return strnull;
    }
}

static int args_parse_int(cmd_args_t *args)
{
    string_t arg = args_next(args);

    char *end;
    int result = (int)strtol(arg.data, &end, 0);

    if (end == arg.data)
    {
        args_error(args, strlit("argument must be an integer"));
    }

    return result;
}

static float args_parse_float(cmd_args_t *args)
{
    string_t arg = args_next(args);

    char *end;
    float result = strtof(arg.data, &end);

    if (end == arg.data)
    {
        args_error(args, strlit("argument must be an integer"));
    }

    return result;
}

static void skip_arg(cmd_args_t *args)
{
    args->at += 1;
}

static void print_usage(void)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: build [options]\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "\t-nocache                 disable incremental compilation\n");
    fprintf(stderr, "\t-all                     build all configurations\n");
    fprintf(stderr, "\t-release                 use release config\n");
    fprintf(stderr, "\t-ndebug                  disable debug facilities\n");
    fprintf(stderr, "\t-stub                    use single-translation unit build\n");
    fprintf(stderr, "\t-compiler [msvc/clang]   select compiler to use\n");
}

int main(int argc, char **argv)
{
    // ==========================================================================================================================
    // parse arguments

    bool build_all = false;
    bool no_cache  = false;
    bool ndebug    = false;
    bool release   = false;
    bool stub      = false;
    bool not_slow  = false;
    bool asan      = false;
    backend_compiler_t backend = BACKEND_MSVC;

    cmd_args_t *args = &(cmd_args_t){0};
    init_args(args, argc, argv);

    while (args_left(args))
    {
        if (args_match(args, "-nocache"))
        {
            no_cache = true;
        }
        else if (args_match(args, "-release"))
        {
            release = true;
        }
        else if (args_match(args, "-all"))
        {
            build_all = true;
        }
        else if (args_match(args, "-ndebug"))
        {
            ndebug = true;
        }
        else if (args_match(args, "-not_slow"))
        {
            not_slow = true;
        }
        else if (args_match(args, "-asan"))
        {
            asan = true;
        }
        else if (args_match(args, "-stub"))
        {
            stub = true;
        }
        else if (args_match(args, "-compiler"))
        {
            string_t compiler = args_next(args);
            if (!args->error)
            {
                if (string_match(compiler, strlit("msvc")))
                {
                    backend = BACKEND_MSVC;
                }
                else if (string_match(compiler, strlit("clang")))
                {
                    backend = BACKEND_CLANG;
                }
                else
                {
                    args_error(args, string_format(temp, "unknown compiler %.*s\n", strexpand(compiler)));
                }
            }
        }
        else
        {
            args_error(args, string_format(temp, "unknown argument '%s'\n", *args->at));
            args->at += 1;
        }
    }

    if (args->error)
    {
        print_usage();
        return 1;
    }

    hires_time_t total_time_start = os_hires_time();

    // ==========================================================================================================================
    // build source

    printf("\n");
    printf("=================================================\n");
    printf("                  BUILD SOURCE                   \n");
    printf("=================================================\n");
    printf("\n");

    build_result_t result = BUILD_SUCCESS;

    build_job_t base_job = {
        .no_std_lib              = false,

        .no_cache                = true, // incremental compilation is kaput
        .single_translation_unit = stub,

        .backend                 = backend,

        .address_sanitizer       = asan,

        .warnings_are_errors     = true,
        .warning_level           = W4,
        .optimization_level      = O0,

        .output_exe              = strlit("retro.exe"),
        .configuration           = strlit("debug"),

        .libraries = slist_from_array(temp, array_expand(string_t, 
            strlit("user32"),
            strlit("dxguid"),
            strlit("d3d11"),
            strlit("dxgi"),
            strlit("d3dcompiler")
        )),

        .defines = slist_from_array(temp, array_expand(string_t,
            strlit("_CRT_SECURE_NO_WARNINGS"),
            strlit("UNICODE"),
            ndebug ? strlit("NDEBUG") : strlit("DEBUG"),
            not_slow ? strlit("DEBUG_FAST") : strlit("DEBUG_SLOW"),
        )),

        .include_paths = slist_from_array(temp, array_expand(string_t,
            strlit("src"),
            strlit("external/include")
        )),

        .ignored_directories = slist_from_array(temp, array_expand(string_t,
            strlit("buildtool"),
            strlit("DONTBUILD")
        )),
    };

    build_job_t release_job = base_job;
    release_job.optimization_level = O2;
    release_job.configuration      = strlit("release");

    if (!release || build_all)
    {
        result |= build_directory(strlit("src"), &base_job);

        switch (result)
        {
            case BUILD_SUCCESS:           fprintf(stderr, "Build successful\n"); break;
            case BUILD_OTHER_FAILURE:     fprintf(stderr, "Build failed for other reasons! Oh no!\n"); break;
            case BUILD_COMPILATION_ERROR: fprintf(stderr, "Build failed with compilation errors\n"); break;
            case BUILD_LINKER_ERROR:      fprintf(stderr, "Build failed with linker errors\n"); break;
        }
    }

    if ((release || build_all) && result == BUILD_SUCCESS)
    {
        result |= build_directory(strlit("src"), &release_job);

        switch (result)
        {
            case BUILD_SUCCESS:           fprintf(stderr, "Build successful\n"); break;
            case BUILD_OTHER_FAILURE:     fprintf(stderr, "Build failed for other reasons! Oh no!\n"); break;
            case BUILD_COMPILATION_ERROR: fprintf(stderr, "Build failed with compilation errors\n"); break;
            case BUILD_LINKER_ERROR:      fprintf(stderr, "Build failed with linker errors\n"); break;
        }
    }

    // ==========================================================================================================================
    // asset processing

    if (result == BUILD_SUCCESS)
    {
        build_maps(strlit("assets/maps"), strlit("run/gamedata/maps"));
    }

    // ==========================================================================================================================

    hires_time_t total_time_end = os_hires_time();

    string_t total_running_time = format_seconds(temp, os_seconds_elapsed(total_time_start, total_time_end));
    fprintf(stderr, "\nTOTAL BUILD TIME: %.*s\n", strexpand(total_running_time));
}
