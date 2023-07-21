#define UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

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
#include "core/args_parser.h"

#include "backend_msvc.c"
#include "backend_clang.c"
#include "build.c"
#include "assets.c"

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

    bool build_all  = true;
    bool no_cache   = false;
    bool ndebug     = false;
    bool release    = false;
    bool stub       = false;
	bool nomodules  = false;
    bool not_slow   = false;
    bool asan       = false;
    backend_compiler_t backend = BACKEND_MSVC;

    cmd_args_t *args = &(cmd_args_t){0};
    init_args(args, argc, argv);

    while (args_left(args))
    {
        if (args_match(args, "-nocache"))
        {
            no_cache = true;
        }
        else if (args_match(args, "-debug"))
        {
            release = false;
			build_all = false;
        }
        else if (args_match(args, "-release"))
        {
            release = true;
			build_all = false;
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
        else if (args_match(args, "-nomodules"))
        {
            nomodules = true;
        }
        else if (args_match(args, "-compiler"))
        {
            string_t compiler = args_next(args);
            if (!args->error)
            {
                if (string_match(compiler, S("msvc")))
                {
                    backend = BACKEND_MSVC;
                }
                else if (string_match(compiler, S("clang")))
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

	if (nomodules)
	{		
		printf("\n");
		printf("=================================================\n");
		printf("                  BUILDING SOURCE                \n");
		printf("=================================================\n");
		printf("\n");

		build_error_t error = BUILD_ERROR_NONE;

		all_params_t base_params = {
			.build = {
				.no_cache                = true, // incremental compilation is kaput

				.configuration           = S("debug"),
				.backend                 = backend,
			},

			.compile = {
				.single_translation_unit = stub,
				.stub_name               = S("dream"),

				.address_sanitizer       = asan,

				.warnings_are_errors     = true,
				.warning_level           = W4,

				.optimization_level      = O0,

				.defines = slist_from_array(temp, array_expand(string_t,
					S("_CRT_SECURE_NO_WARNINGS"),
					S("UNICODE"),
					ndebug ? S("NDEBUG") : S("DEBUG"),
					not_slow ? S("DREAM_FAST") : S("DREAM_SLOW"),
				)),

				.include_paths = slist_from_array(temp, array_expand(string_t,
					S("src"),
					S("external/include")
				)),

				.ignored_directories = slist_from_array(temp, array_expand(string_t,
					S("buildtool"),
					S("DONTBUILD")
				)),
			},

			.link = {
				.output_exe              = S("retro"),
				.run_dir                 = S("run"),
				.copy_executables_to_run = true,

				.libraries = slist_from_array(temp, array_expand(string_t, 
					S("user32"),
					S("dxguid"),
					S("d3d11"),
					S("dxgi"),
					S("d3dcompiler"),
					S("gdi32"),
					S("user32"),
					S("ole32"),
					S("ksuser"),
					S("shell32"),
					S("Synchronization")
				)),
			},
		};

		build_context_t *context = m_alloc_struct(temp, build_context_t);
		context->arena     = temp;
		context->params    = &base_params.build;
		context->build_dir = Sf("build/%.*s", Sx(base_params.build.configuration));

		fs_create_directory(context->build_dir);

		all_params_t release_params = base_params;
		release_params.build.configuration = S("release");
		release_params.compile.optimization_level = O2;

		slist_appends(&base_params   .compile.defines, temp, S("DREAM_UNOPTIMIZED=1"));
		slist_appends(&release_params.compile.defines, temp, S("DREAM_OPTIMIZED=1"));

		if (!release || build_all)
		{
			error |= build_directory(context, S("src"), &base_params);

			switch (error)
			{
				case BUILD_ERROR_NONE:          fprintf(stderr, "Build successful\n"); break;
				case BUILD_ERROR_OTHER_FAILURE: fprintf(stderr, "Build failed for other reasons! Oh no!\n"); break;
				case BUILD_ERROR_COMPILATION:   fprintf(stderr, "Build failed with compilation errors\n"); break;
				case BUILD_ERROR_LINKER:        fprintf(stderr, "Build failed with linker errors\n"); break;
			}
		}

		if ((release || build_all) && !error)
		{
			error |= build_directory(context, S("src"), &release_params);

			switch (error)
			{
				case BUILD_ERROR_NONE:          fprintf(stderr, "Build successful\n"); break;
				case BUILD_ERROR_OTHER_FAILURE: fprintf(stderr, "Build failed for other reasons! Oh no!\n"); break;
				case BUILD_ERROR_COMPILATION:   fprintf(stderr, "Build failed with compilation errors\n"); break;
				case BUILD_ERROR_LINKER:        fprintf(stderr, "Build failed with linker errors\n"); break;
			}
		}

	}
	else
	{

		for (int config = 0; config < 2; config++)
		{
			bool is_debug   = config == 0;
			bool is_release = config == 1;

			if (is_debug)
			{
				printf("\n");
				printf("=================================================\n");
				printf("                BUILDING DEBUG                   \n");
				printf("=================================================\n");
				printf("\n");
			}
			else if (is_release)
			{
				printf("\n");
				printf("=================================================\n");
				printf("                BUILDING RELEASE                 \n");
				printf("=================================================\n");
				printf("\n");
			}
			else
			{
				INVALID_CODE_PATH;
			}

			string_t source_directory = S("src");

			bool build_failed = false;

			object_collection_t objects = {0};

			build_params_t build = {
				.no_cache      = true, // incremental compilation is kaput

				.configuration = is_debug ? S("debug") : S("release"),
				.backend       = backend,
			};

			build_context_t *context = m_alloc_struct(temp, build_context_t);
			context->arena     = temp;
			context->params    = &build;
			context->build_dir = Sf("build/%.*s", Sx(build.configuration));

			fs_create_directory(context->build_dir);

			compile_params_t compile = {
				.single_translation_unit = false,

				.address_sanitizer       = asan,

				.warnings_are_errors     = true,
				.warning_level           = W4,

				.optimization_level      = is_debug ? O0 : O2,

				.defines = slist_from_array(temp, array_expand(string_t,
					S("_CRT_SECURE_NO_WARNINGS"),
					S("UNICODE"),
					ndebug   ? S("NDEBUG")     : S("DEBUG"),
					not_slow ? S("DREAM_FAST") : S("DREAM_SLOW"),
				)),

				.include_paths = slist_from_array(temp, array_expand(string_t,
					S("src"),
					S("external/include")
				)),

				.ignored_directories = slist_from_array(temp, array_expand(string_t,
					S("buildtool"),
					S("DONTBUILD")
				)),
			};

			if (is_debug)   slist_appends(&compile.defines, temp, S("DREAM_UNOPTIMIZED=1"));
			if (is_release) slist_appends(&compile.defines, temp, S("DREAM_OPTIMIZED=1"));

			source_files_t module_files = {0};

			for (fs_entry_t *entry = fs_scan_directory(temp, source_directory, 0);
				 entry;
				 entry = fs_entry_next(entry))
			{
				if (string_match_nocase(entry->name, S("buildtool")))
					continue;

				source_files_t files = {0};
				gather_source_files(temp, entry->path, &compile, &files);
				transform_into_stub(context, &files, entry->name);

				dll_push_back(module_files.first, module_files.last, files.first);
			}

			compile_error_t compile_error = compile_files(context, module_files, &build, &compile, &objects);

			if (compile_error)
			{
				fprintf(stderr, "Compilation failed! TODO: More information\n");

				build_failed = true;
				break;
			}

			if (!build_failed)
			{
				link_params_t link = {
					.output_exe              = S("retro"),
					.run_dir                 = S("run"),
					.copy_executables_to_run = true,

					.libraries = slist_from_array(temp, array_expand(string_t, 
						S("user32"),
						S("dxguid"),
						S("d3d11"),
						S("dxgi"),
						S("d3dcompiler"),
						S("gdi32"),
						S("user32"),
						S("ole32"),
						S("ksuser"),
						S("shell32"),
						S("Synchronization")
					)),
				};

				link_error_t link_error = link_executable(context, &objects, &build, &link);

				if (link_error)
				{
					fprintf(stderr, "Linking failed! TODO: More information\n");
					build_failed = true;
					break;
				}
			}
		}
	}

    // ==========================================================================================================================

    hires_time_t total_time_end = os_hires_time();

    string_t total_running_time = format_seconds(temp, os_seconds_elapsed(total_time_start, total_time_end));
    fprintf(stderr, "\nTOTAL BUILD TIME: %.*s\n", strexpand(total_running_time));

	// TODO: print out most of this per build job rather than at the end

	fprintf(stderr, "built configurations: ");
	if (build_all || !release) fprintf(stderr, "debug ");
	if (build_all ||  release) fprintf(stderr, "release ");
	fprintf(stderr, "\n");

	fprintf(stderr, "build options: ");
	if (asan)     fprintf(stderr, "-asan ");
	if (ndebug)   fprintf(stderr, "-ndebug ");
	if (not_slow) fprintf(stderr, "-not_slow ");
	if (stub)     fprintf(stderr, "-stub ");
	fprintf(stderr, "\n");

	char *backend_name = "unknown";
	switch (backend)
	{
		case BACKEND_MSVC:  backend_name = "msvc";  break;
		case BACKEND_CLANG: backend_name = "clang"; break;
	}
	fprintf(stderr, "compiler: %s\n", backend_name);
}
