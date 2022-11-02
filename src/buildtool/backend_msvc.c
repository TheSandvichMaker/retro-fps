static string_t msvc_warning_to_string[WARNING_COUNT] = {
    [WARNING_ANONYMOUS_STRUCT] = strinit("/wd4201"),
};

static build_result_t msvc_build(build_context_t *context, const source_files_t *files_, const build_job_t *job)
{
    build_result_t result = BUILD_SUCCESS;

    source_files_t *files = m_copy_struct(context->arena, files_); // just wanna avoid modifying the argument

    string_list_t cl_flags_shared = { 0 };
    slist_appendf(&cl_flags_shared, temp, "/Fo%.*s/", strexpand(context->build_dir));
    slist_appends(&cl_flags_shared, temp, strlit("/nologo"));

    for (string_node_t *node = job->defines.first; node; node = node->next)
    {
        slist_appendf(&cl_flags_shared, temp, "/D%.*s", strexpand(node->string));
    }

    for (string_node_t *node = job->include_paths.first; node; node = node->next)
    {
        slist_appendf(&cl_flags_shared, temp, "/I\"%.*s\"", strexpand(node->string));
    }
    slist_appendf(&cl_flags_shared, temp, "/I.");

    string_list_t cl_flags = { 0 };
    slist_appends(&cl_flags, temp, strlit("/Zo"));
    slist_appends(&cl_flags, temp, strlit("/Zi"));
    slist_appends(&cl_flags, temp, strlit("/Oi"));
    slist_appends(&cl_flags, temp, strlit("/fp:except-"));
    slist_appends(&cl_flags, temp, strlit("/std:c11"));
    slist_appends(&cl_flags, temp, strlit("/MP"));

    switch (job->optimization_level)
    {
        case O0: slist_appends(&cl_flags, temp, strlit("/Od")); break;
        case O1: slist_appends(&cl_flags, temp, strlit("/O1")); break;
        case O2: slist_appends(&cl_flags, temp, strlit("/O2")); break;
        INVALID_DEFAULT_CASE;
    }

    switch (job->warning_level)
    {
        case W0: slist_appends(&cl_flags, temp, strlit("/W0")); break;
        case W1: slist_appends(&cl_flags, temp, strlit("/W1")); break;
        case W2: slist_appends(&cl_flags, temp, strlit("/W2")); break;
        case W3: slist_appends(&cl_flags, temp, strlit("/W3")); break;
        case W4: slist_appends(&cl_flags, temp, strlit("/W4")); break;
        INVALID_DEFAULT_CASE;
    }

    for (size_t i = 0; i < ARRAY_COUNT(job->warnings); i++)
    {
        bool enabled = job->warnings[i] == WARNING_STATE_ENABLED;

        if (job->warnings[i] == WARNING_STATE_SANE_DEFAULT)
            enabled = warning_defaults[i] == WARNING_STATE_ENABLED;

        if (enabled)
            slist_appends(&cl_flags, temp, msvc_warning_to_string[i]);
    }

    if (job->warnings_are_errors)
        slist_appends(&cl_flags, temp, strlit("/WX"));

    if (job->address_sanitizer)
        slist_appends(&cl_flags, temp, strlit("/fsanitize=address"));

    if (job->no_std_lib)
    {
        slist_appends(&cl_flags, temp, strlit("/GS-"));
        slist_appends(&cl_flags, temp, strlit("/Gs9999999"));

        string_t crt_support_string = strlit(
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

        source_file_t *crt_support = generate_file(context, strlit("crt_support.c"), crt_support_string);
        dll_push_back(files->first, files->last, crt_support);
    }


    slist_interleaves(&cl_flags_shared, temp, strlit(" "));
    string_t cl_flags_shared_string = slist_flatten(&cl_flags_shared, temp);

    slist_interleaves(&cl_flags, temp, strlit(" "));
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
        slist_interleaves(&list, temp, strlit(" "));
        source_string = slist_flatten(&list, temp);
    }

    if (source_string.count > 0)
    {
        string_t cl_string = string_format(temp, "cl /c %.*s %.*s %.*s", strexpand(cl_flags_shared_string), strexpand(cl_flags_string), strexpand(source_string));

        fprintf(stderr, "Running command: %.*s\n\n", strexpand(cl_string));

        hires_time_t compiler_start = os_hires_time();

        int exit_code;
        if (os_execute(cl_string, &exit_code))
        {
            hires_time_t compiler_end = os_hires_time();

            string_t compiler_time = format_seconds(temp, os_seconds_elapsed(compiler_start, compiler_end));
            fprintf(stderr, "\ncl.exe ran in %.*s\n", strexpand(compiler_time));

            if (exit_code == 0)
            {
                push_working_directory(context->build_dir)
                {
                    string_t obj_string; 
                    {
                        string_list_t obj_files = { 0 };
                        for (source_file_t *file = files->first; file; file = file->next)
                        {
                            string_t stripped = string_strip_extension(file->name);
                            slist_appendf(&obj_files, temp, "%.*s.obj", strexpand(stripped));
                        }
                        slist_interleaves(&obj_files, temp, strlit(" "));
                        obj_string = slist_flatten(&obj_files, temp);
                    }

                    string_t link_flags;
                    {
                        string_list_t link_flags_list = { 0 };
                        slist_appendf(&link_flags_list, temp, "/DEBUG:FULL");
                        slist_appendf(&link_flags_list, temp, "/SUBSYSTEM:WINDOWS");
                        slist_appendf(&link_flags_list, temp, "/OUT:\"%.*s\"", strexpand(job->output_exe));

                        for (string_node_t *lib = job->libraries.first; lib; lib = lib->next)
                        {
                            slist_appendf(&link_flags_list, temp, "%.*s.lib", strexpand(lib->string));
                        }

                        if (job->no_std_lib)
                        {
                            slist_appends(&link_flags_list, temp, strlit("/NODEFAULTLIB"));
                            slist_appends(&link_flags_list, temp, strlit("/STACK:0x100000,0x100000"));
                            slist_appends(&link_flags_list, temp, strlit("kernel32.lib"));
                        }

                        slist_interleaves(&link_flags_list, temp, strlit(" "));
                        link_flags = slist_flatten(&link_flags_list, temp);
                    }

                    string_t link_string = string_format(temp, "link /nologo %.*s %.*s", strexpand(link_flags), strexpand(obj_string));

                    fprintf(stderr, "\nRunning command: %.*s\n", strexpand(link_string));

                    hires_time_t link_start = os_hires_time();

                    if (os_execute(link_string, &exit_code))
                    {
                        if (exit_code == 0)
                        {
                            // good

                            hires_time_t link_end = os_hires_time();

                            string_t link_time = format_seconds(temp, os_seconds_elapsed(link_start, link_end));
                            fprintf(stderr, "\nlink.exe ran in %.*s\n", strexpand(link_time));
                        }
                        else
                        {
                            fprintf(stderr, "\nLINKER FAILED: link.exe exited with code %d\n", exit_code);
                            result = BUILD_LINKER_ERROR;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "\nFailed to run link.exe!\n");
                        result = BUILD_OTHER_FAILURE;
                    }
                }
            }
            else
            {
                fprintf(stderr, "\nCOMPILATION FAILED: cl.exe exited with code %d\n", exit_code);
                result = BUILD_COMPILATION_ERROR;
            }
        }
        else
        {
            fprintf(stderr, "Failed to run cl.exe!\n");
            result = BUILD_OTHER_FAILURE;
        }
    }
    else
    {
        fprintf(stderr, "Nothing needs to be rebuilt.\n");
    }

    return result;
}

static backend_i backend_msvc = {
    .build = msvc_build,
};
