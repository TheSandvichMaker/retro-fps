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
DREAM_INLINE string_t format_seconds(arena_t *arena, double seconds);

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

DREAM_INLINE string_t format_seconds(arena_t *arena, double seconds)
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

static build_error_t build_plugin(build_context_t *context, build_params_t *build, compile_params_t *compile, string_t name, string_list_t dependencies__remove_me)
{
	build_error_t result = BUILD_ERROR_NONE;

	source_files_t dll_files = {0};

	string_list_t directories = dependencies__remove_me;
	slist_appends(&directories, temp, Sf("plugins/%.*s", Sx(name)));

	for (string_node_t *node = directories.first;
		 node;
		 node = node->next)
	{
		string_t directory = node->string;
		gather_source_files(temp, Sf("src/%.*s", Sx(directory)), compile, &dll_files);
	}

	object_collection_t dll_objects = {0};
	compile_error_t compile_error = compile_files(context, dll_files, build, compile, &dll_objects);

	if (compile_error)
	{
		result = BUILD_ERROR_COMPILATION;
	}
	else
	{
		link_params_t link_dll = {
			.kind                    = LINK_ARTEFACT_DYNAMIC_LIBRARY,
			.output                  = name,
			.run_dir                 = S("run"),
			.copy_executables_to_run = true,
		};

		link_error_t link_error = link_executable(context, &dll_objects, build, &link_dll);

		if (link_error)
		{
			result = BUILD_ERROR_LINKER;
		}
	}

	return result;
}

int main(int argc, char **argv)
{
    // ==========================================================================================================================
    // parse arguments

    bool build_all  = true;
    bool no_cache   = false;
    bool ndebug     = false;
    bool debug      = true;
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
            debug = false;
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
                    args_error(args, Sf("unknown compiler %.*s\n", Sx(compiler)));
                }
            }
        }
        else
        {
            args_error(args, Sf("unknown argument '%s'\n", *args->at));
			skip_arg(args);
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

	{
		bool build_failed = false;

		for (int config = 0; config < 2; config++)
		{
			if (build_failed)
				break;

			bool is_debug   = config == 0;
			bool is_release = config == 1;

			if (!build_all && debug   && !is_debug)   continue;
			if (!build_all && release && !is_release) continue;

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
					S("DREAM_WIN32=1"),
					S("_CRT_SECURE_NO_WARNINGS"),
					S("UNICODE"),
					ndebug   ? S("NDEBUG")       : S("DEBUG"),
					not_slow ? S("DREAM_SLOW=0") : S("DREAM_SLOW=1"),
				)),

				.include_paths = slist_from_array(temp, array_expand(string_t,
					source_directory,
					S("external/include")
				)),

				.ignored_directories = slist_from_array(temp, array_expand(string_t,
					S("buildtool"),
					S("DONTBUILD")
				)),
			};

			if (is_debug)   slist_appends(&compile.defines, temp, S("DREAM_UNOPTIMIZED=1"));
			if (is_release) slist_appends(&compile.defines, temp, S("DREAM_OPTIMIZED=1"));

			source_files_t exe_files = {0};

			string_t exe_source_directories[] = {
				S("core"),
				S("dream"),
				S("win32"),
			};

			for_array(i, exe_source_directories)
			{
				string_t directory = exe_source_directories[i];

				if (NEVER(string_match_nocase(directory, S("buildtool"))))
					continue;

				source_files_t files = {0};
				gather_source_files(temp, Sf("%.*s/%.*s", Sx(source_directory), Sx(directory)), &compile, &files);
				transform_into_stub(context, &files, directory);

				dll_push_back(exe_files.first, exe_files.last, files.first);
			}

			compile_error_t compile_error = COMPILE_ERROR_NONE;

			object_collection_t exe_objects = {0};
			compile_error |= compile_files(context, exe_files, &build, &compile, &exe_objects);

			if (compile_error)
			{
				fprintf(stderr, "Compilation failed! TODO: More information\n");

				build_failed = true;
			}

			{
				link_params_t link_exe = {
					.kind                    = LINK_ARTEFACT_EXECUTABLE,
					.output                  = S("win32_retro"),
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
						S("Synchronization"),
						S("DbgHelp")
					)),
				};

				link_error_t link_error = link_executable(context, &exe_objects, &build, &link_exe);

				if (link_error)
				{
					fprintf(stderr, "Linking failed! TODO: More information\n");
					build_failed = true;
				}
			}

			//
			// plugins
			//

			if (build_plugin(context, &build, &compile, S("fs"), slist_from_array(temp, array_expand(string_t, S("core")))) != BUILD_ERROR_NONE)
			{
				fprintf(stderr, "Failed to build fs plugin\n");
				build_failed = true;
			}

			if (build_plugin(context, &build, &compile, S("audio_output"), (string_list_t){0}) != BUILD_ERROR_NONE)
			{
				fprintf(stderr, "Failed to audio output plugin\n");
				build_failed = true;
			}
		}
	}

    // ==========================================================================================================================
	// copy shaders

#define ROBOCOPY_SILENT " /NFL /NDL /NJH /NJS /NC /NS /NP"

	// TODO: Figure out good directory mirroring that doesn't require calling an external process.
	int robocopy_exit_code;
	os_execute(S("robocopy src/shaders run/gamedata/shaders /MIR" ROBOCOPY_SILENT), &robocopy_exit_code);

	if (robocopy_exit_code >= 4)
	{
		fprintf(stderr, "Failed to copy shaders! Robocopy returned error code 0x%X. Meaning given below:\n", robocopy_exit_code);
		if (robocopy_exit_code & 0x04) fprintf(stderr, "ROBOCOPY 0x04: Some mismatched files or directories were detected. Examine the output log.\n");
		if (robocopy_exit_code & 0x08) fprintf(stderr, "ROBOCOPY 0x08: Some files or directories could not be copied.\n");
		if (robocopy_exit_code & 0x10) fprintf(stderr, "ROBOCOPY 0x10: Serious error. Robocopy did not copy any files.\n");
	}
	else
	{
		printf("Copied shaders successfully!\n");
	}

    // ==========================================================================================================================

    hires_time_t total_time_end = os_hires_time();

    string_t total_running_time = format_seconds(temp, os_seconds_elapsed(total_time_start, total_time_end));
    fprintf(stderr, "\nTOTAL BUILD TIME: %.*s\n", Sx(total_running_time));

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
