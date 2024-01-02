static string_t msvc_warning_to_string[WARNING_COUNT] = {
    [WARNING_NONSTANDARD_EXT_ANONYMOUS_STRUCT]       = Sc("/wd4201"),
    [WARNING_NONSTANDARD_EXT_EMPTY_TRANSLATION_UNIT] = Sc("/wd4206"),
    [WARNING_TYPE_DEFINITION_IN_PARENTHESIS]         = Sc("/wd4115"),
    [WARNING_UNNAMED_TYPE_DEFINITION_IN_PARENTHESES] = Sc("/wd4116"),
    [WARNING_FLEXIBLE_ARRAY_MEMBER]                  = Sc("/wd4200"),
};

static compile_error_t msvc_compile(build_context_t *context, const compile_params_t *compile, const source_files_t *files_, object_collection_t *objects)
{
    compile_error_t result = COMPILE_ERROR_NONE;

	const build_params_t *build = context->params;

    source_files_t *files = m_copy_struct(context->arena, files_); // just wanna avoid modifying the argument

    string_list_t cl_flags_shared = { 0 };
    slist_appendf(&cl_flags_shared, temp, "/Fo%.*s/", Sx(context->build_dir));
    slist_appends(&cl_flags_shared, temp, S("/nologo"));

    for (string_node_t *node = compile->defines.first; node; node = node->next)
    {
        slist_appendf(&cl_flags_shared, temp, "/D%.*s", Sx(node->string));
    }

    for (string_node_t *node = compile->include_paths.first; node; node = node->next)
    {
        slist_appendf(&cl_flags_shared, temp, "/I\"%.*s\"", Sx(node->string));
    }
    slist_appendf(&cl_flags_shared, temp, "/I.");

    string_list_t cl_flags = { 0 };
    slist_appends(&cl_flags, temp, S("/Zo"));
    slist_appends(&cl_flags, temp, S("/Zi"));
    slist_appends(&cl_flags, temp, S("/Oi"));
    slist_appends(&cl_flags, temp, S("/fp:except-"));
    slist_appends(&cl_flags, temp, S("/std:c11"));
    slist_appends(&cl_flags, temp, S("/MP"));

    switch (compile->optimization_level)
    {
        case O0: slist_appends(&cl_flags, temp, S("/Od")); break;
        case O1: slist_appends(&cl_flags, temp, S("/O1")); break;
        case O2: slist_appends(&cl_flags, temp, S("/O2")); break;
        INVALID_DEFAULT_CASE;
    }

    switch (compile->warning_level)
    {
        case W0: slist_appends(&cl_flags, temp, S("/W0")); break;
        case W1: slist_appends(&cl_flags, temp, S("/W1")); break;
        case W2: slist_appends(&cl_flags, temp, S("/W2")); break;
        case W3: slist_appends(&cl_flags, temp, S("/W3")); break;
        case W4: slist_appends(&cl_flags, temp, S("/W4")); break;
        INVALID_DEFAULT_CASE;
    }

    for (size_t i = 0; i < ARRAY_COUNT(compile->warnings); i++)
    {
        bool enabled = compile->warnings[i] == WARNING_STATE_ENABLED;

        if (compile->warnings[i] == WARNING_STATE_SANE_DEFAULT)
            enabled = warning_defaults[i] == WARNING_STATE_ENABLED;

        if (!enabled)
            slist_appends(&cl_flags, temp, msvc_warning_to_string[i]);
    }

    if (compile->warnings_are_errors)
        slist_appends(&cl_flags, temp, S("/WX"));

    if (compile->address_sanitizer)
        slist_appends(&cl_flags, temp, S("/fsanitize=address"));

    if (build->no_std_lib)
    {
        slist_appends(&cl_flags, temp, S("/GS-"));
        slist_appends(&cl_flags, temp, S("/Gs9999999"));

        string_t crt_support_string = S(
            "#include <stddef.h>"                                                   "\n"
            ""                                                                      "\n"
            "int _fltused;"                                                         "\n"
            ""                                                                      "\n"
            "#pragma function(memset)"                                              "\n"
            "void *memset(void *dest, int c, size_t count)"                         "\n"
            "{"                                                                     "\n"
            "   char *bytes = (char *)dest;"                                        "\n"
            "   while (count--)"                                                    "\n"
            "   {"                                                                  "\n"
            "       *bytes++ = (char)c;"                                            "\n"
            "   }"                                                                  "\n"
            "   return dest;"                                                       "\n"
            "}"                                                                     "\n"
            ""                                                                      "\n"
            "#pragma function(memcpy)"                                              "\n"
            "void *memcpy(void *dest, const void *src, size_t count)"               "\n"
            "{"                                                                     "\n"
            "   char *dest8 = (char *)dest;"                                        "\n"
            "   const char *src8 = (const char *)src;"                              "\n"
            "   while (count--)"                                                    "\n"
            "   {"                                                                  "\n"
            "       *dest8++ = *src8++;"                                            "\n"
            "   }"                                                                  "\n"
            "   return dest;"                                                       "\n"
            "}"                                                                     "\n"
            ""                                                                      "\n"
            "#define WIN32_LEAN_AND_MEAN"                                           "\n"
            "#include <windows.h>"                                                  "\n"
            "#include <shellapi.h>"                                                 "\n"
            ""                                                                      "\n"
            "int CALLBACK wWinMain(HINSTANCE instance, HINSTANCE prev_instance, PWSTR command_line, int command_show);" "\n"
            ""                                                                      "\n"
            "void __stdcall wWinMainCRTStartup()"                                   "\n"
            "{"                                                                     "\n"
            "    int result = wWinMain(NULL, NULL, GetCommandLineW(), SW_SHOW);"    "\n"
            "    ExitProcess(result);"                                              "\n"
            "}"                                                                     "\n"
        );

        source_file_t *crt_support = generate_file(context, S("crt_support.c"), crt_support_string);
        dll_push_back(files->first, files->last, crt_support);
    }


    slist_interleaves(&cl_flags_shared, temp, S(" "));
    string_t cl_flags_shared_string = slist_flatten(&cl_flags_shared, temp);

    slist_interleaves(&cl_flags, temp, S(" "));
    string_t cl_flags_string = slist_flatten(&cl_flags, temp);

    string_t source_string;
    {
        string_list_t list = { 0 };
        for (source_file_t *file = files->first; file; file = file->next)
        {
            if (!file->cached)
            {
                slist_appends(&list, temp, file->path);
            }
        }
        slist_interleaves(&list, temp, S(" "));
        source_string = slist_flatten(&list, temp);
    }

    if (source_string.count > 0)
    {
        string_t cl_string = Sf("cl /c %.*s %.*s %.*s", Sx(cl_flags_shared_string), Sx(cl_flags_string), Sx(source_string));

        fprintf(stderr, "Running command: %.*s\n\n", Sx(cl_string));

        hires_time_t compiler_start = os_hires_time();

        int exit_code;
        if (os_execute(cl_string, &exit_code))
        {
            hires_time_t compiler_end = os_hires_time();

            string_t compiler_time = format_seconds(temp, os_seconds_elapsed(compiler_start, compiler_end));
            fprintf(stderr, "\ncl.exe ran in %.*s\n", Sx(compiler_time));

            if (exit_code == 0)
            {
				for (source_file_t *file = files->first;
					 file;
					 file = file->next)
				{
					string_t base_name = string_strip_extension(file->name);

					object_t *obj = m_alloc_struct(context->arena, object_t);
					obj->name = string_format(context->arena, "%.*s.obj", Sx(base_name));
					dll_push_back(objects->first, objects->last, obj);
				}
			}
			else
			{
                fprintf(stderr, "\nCOMPILATION FAILED: cl.exe exited with code %d\n", exit_code);
                result = COMPILE_ERROR_OTHER_FAILURE;
            }
        }
        else
        {
            fprintf(stderr, "Failed to run cl.exe!\n");
            result = COMPILE_ERROR_OTHER_FAILURE;
        }
    }
    else
    {
        fprintf(stderr, "Nothing needs to be rebuilt.\n");
    }

    return result;
}

static link_error_t msvc_link(build_context_t *context, const link_params_t *params, const object_collection_t *objects)
{
    link_error_t result = LINK_ERROR_NONE;

	const build_params_t *build = context->params;

	push_working_directory(context->build_dir)
	{
		string_t obj_string; 
		{
			string_list_t list = { 0 };
			for (object_t *obj = objects->first;
				 obj;
				 obj = obj->next)
			{
				slist_appends_nocopy(&list, temp, obj->name);
			}
			slist_interleaves(&list, temp, S(" "));
			obj_string = slist_flatten(&list, temp);
		}

		string_t link_flags;
		{
			string_t ext = S("exe");
			switch (params->kind)
			{
				case LINK_ARTEFACT_EXECUTABLE:
				{
					ext = S("exe");
				} break;

				case LINK_ARTEFACT_DYNAMIC_LIBRARY:
				{
					ext = S("dll");
				} break;

				INVALID_DEFAULT_CASE;
			}

			string_list_t link_flags_list = { 0 };
			slist_appendf(&link_flags_list, temp, "/DEBUG:FULL");
			slist_appendf(&link_flags_list, temp, "/SUBSYSTEM:WINDOWS");
			slist_appendf(&link_flags_list, temp, "/OUT:\"%.*s.%.*s\"", Sx(params->output), Sx(ext));

			for (string_node_t *lib = params->libraries.first; lib; lib = lib->next)
			{
				slist_appendf(&link_flags_list, temp, "%.*s.lib", Sx(lib->string));
			}

			if (build->no_std_lib)
			{
				slist_appends(&link_flags_list, temp, S("/NODEFAULTLIB"));
				slist_appends(&link_flags_list, temp, S("/STACK:0x100000,0x100000"));
				slist_appends(&link_flags_list, temp, S("kernel32.lib"));
			}

			if (params->kind == LINK_ARTEFACT_DYNAMIC_LIBRARY)
			{
				slist_appends(&link_flags_list, temp, S("/DLL"));
			}

			slist_interleaves(&link_flags_list, temp, S(" "));
			link_flags = slist_flatten(&link_flags_list, temp);
		}

		string_t link_string = Sf("link /nologo %.*s %.*s", Sx(link_flags), Sx(obj_string));

		fprintf(stderr, "\nRunning command: %.*s\n", Sx(link_string));

		hires_time_t link_start = os_hires_time();

		int exit_code;
		if (os_execute(link_string, &exit_code))
		{
			if (exit_code == 0)
			{
				// good

				hires_time_t link_end = os_hires_time();

				string_t link_time = format_seconds(temp, os_seconds_elapsed(link_start, link_end));
				fprintf(stderr, "\nlink.exe ran in %.*s\n", Sx(link_time));
			}
			else
			{
				fprintf(stderr, "\nLINKER FAILED: link.exe exited with code %d\n", exit_code);
				result = LINK_ERROR_OTHER_FAILURE;
			}
		}
		else
		{
			fprintf(stderr, "\nFailed to run link.exe!\n");
			result = LINK_ERROR_OTHER_FAILURE;
		}
	}

    return result;
}

static backend_i backend_msvc = {
	.name    = "msvc",
    .compile = msvc_compile,
    .link    = msvc_link,
};
