static wchar_t *format_error(HRESULT error)
{
    wchar_t *message = NULL;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER| 
                   FORMAT_MESSAGE_FROM_SYSTEM|
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   error,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (wchar_t *)&message,
                   0, NULL);

    return message;
}

static void display_last_error(void)
{
    DWORD error = GetLastError() & 0xFFFF;
    wchar_t *message = format_error(error);
    MessageBoxW(0, message, L"Error", MB_OK);
    LocalFree(message);
    __debugbreak();
}

static void output_last_error(string16_t prefix)
{
	wchar_t *message = format_error(GetLastError());

	m_scoped_temp
	OutputDebugStringW(string16_null_terminate(temp, prefix));

	OutputDebugStringW(L": ");
	OutputDebugStringW(message);
	LocalFree(message);
}

static void error_box(char *message)
{
    MessageBoxA(NULL, message, "Error", MB_OK);
}

string16_t utf16_from_utf8(arena_t *arena, string_t utf8)
{
    if (NEVER(utf8.count > INT_MAX))  utf8.count = INT_MAX;

    if (utf8.count == 0)  return (string16_t) { 0 };

    int      utf16_count = MultiByteToWideChar(CP_UTF8, 0, utf8.data, (int)utf8.count, NULL, 0);
    wchar_t *utf16_data  = m_alloc_string16(arena, utf16_count + 1);

    if (ALWAYS(utf16_data))
    {
        MultiByteToWideChar(CP_UTF8, 0, utf8.data, (int)utf8.count, (wchar_t *)utf16_data, utf16_count);
        utf16_data[utf16_count] = 0;
    }

    string16_t result = {
        .count = (size_t)utf16_count,
        .data  = utf16_data,
    };

    return result;
}

string_t utf8_from_utf16(arena_t *arena, string16_t utf16)
{
    if (NEVER(utf16.count > INT_MAX))  utf16.count = INT_MAX;

    if (utf16.count == 0)  return (string_t) { 0 };

    int   utf8_count = WideCharToMultiByte(CP_UTF8, 0, utf16.data, (int)utf16.count, NULL, 0, NULL, NULL);
    char *utf8_data  = m_alloc_string(arena, utf8_count + 1);

    if (ALWAYS(utf8_data))
    {
        WideCharToMultiByte(CP_UTF8, 0, utf16.data, (int)utf16.count, (char *)utf8_data, utf8_count, NULL, NULL);
        utf8_data[utf8_count] = 0;
    }

    string_t result = {
        .count = (size_t)utf8_count,
        .data  = utf8_data,
    };

    return result;
}

void fatal_error(int line, string_t file, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fatal_error_va(line, file, fmt, args);
    va_end(args);
}

void fatal_error_va(int line, string_t file, const char *fmt, va_list args)
{
    char buffer[4096];

    string_t message   = string_format_into_buffer_va(buffer, sizeof(buffer), fmt, args);
    string_t formatted = string_format_into_buffer(buffer + message.count, sizeof(buffer) - message.count, 
                                                   "Fatal error: %.*s\nLine: %d\nFile: %.*s\n", strexpand(message), line, strexpand(file));

    MessageBoxA(NULL, formatted.data, "Fatal Error", MB_OK);

    __debugbreak();

	if (formatted.count > 0)
	{
		// @ThreadSafety: Make sure threads are in a known state before exiting!
		ExitProcess(1);
	}
}

void *vm_reserve(void *address, size_t size)
{
    void *result = VirtualAlloc(address, size, MEM_RESERVE, PAGE_NOACCESS);
    return result;
}

bool vm_commit(void *address, size_t size)
{
    void *result = VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE);

    if (!result)
        output_last_error(strlit16("vm_commit failed"));

    return !!result;
}

void vm_decommit(void *address, size_t size)
{
    VirtualFree(address, size, MEM_DECOMMIT);
}

void vm_release(void *address)
{
    VirtualFree(address, 0, MEM_RELEASE);
}

void debug_print_va(const char *fmt, va_list args)
{
    // TODO: Convert to string16 and use OutputDebugStringW

	m_scoped_temp
	{
		string_t string = string_format_va(temp, fmt, args);
		OutputDebugStringA(string.data);
	}
}

void debug_print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    debug_print_va(fmt, args);

    va_end(args);
}

// filesystem

string_t fs_read_entire_file(arena_t *arena, string_t path)
{
    string_t result = { 0 };

	arena_t *temp = m_get_temp(&arena, 1);

	m_scoped(temp)
	{
		string16_t path16 = utf16_from_utf8(temp, path);

		HANDLE handle = CreateFileW(path16.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (handle != INVALID_HANDLE_VALUE)
		{
			DWORD file_size_high;
			DWORD file_size_low = GetFileSize(handle, &file_size_high);

			if (file_size_high == 0)
			{
				if (file_size_low + 1 <= m_size_remaining_for_align(arena, DEFAULT_STRING_ALIGN))
				{
					char *buffer = m_alloc_nozero(arena, file_size_low + 1, 16);

					DWORD bytes_read;
					if (ReadFile(handle, buffer, file_size_low, &bytes_read, NULL))
					{
						buffer[bytes_read] = 0;

						result.count = bytes_read;
						result.data  = buffer;
					}
					else
					{
						// TODO: Log read failure
					}
				}
				else
				{
					// TODO: Log OOM
				}
			}
			else
			{
				// TODO: log file too big we dumb need to stop being dumb
			}
			CloseHandle(handle);
		}
	}

    return result;
}

bool fs_write_entire_file(string_t path, string_t file)
{
    bool no_errors = true;

    if (NEVER(file.count > UINT32_MAX))
    {
        no_errors  = false;
        file.count = UINT32_MAX; // TODO: Remove filesize limitation
    }

    m_scoped_temp
    {
        string16_t path16 = utf16_from_utf8(temp, path);

        HANDLE handle = CreateFileW(path16.data, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle != INVALID_HANDLE_VALUE)
        {
            DWORD written;
            if (WriteFile(handle, file.data, (DWORD)file.count, &written, NULL))
            {
                CloseHandle(handle);
            }
            else
            {
                no_errors = false;
                // TODO: Log file write failure
            }
        }
        else
        {
            no_errors = false;
            // TODO: Log file open failure
        }
    }

    return !no_errors;
}

bool fs_copy(string_t source, string_t destination)
{
	bool result = false;

	m_scoped_temp
	{
		string16_t src16 = utf16_from_utf8(temp, source);
		string16_t dst16 = utf16_from_utf8(temp, destination);

		result = CopyFile(src16.data, dst16.data, FALSE);
		if (!result)  output_last_error(strlit16("fs_copy failed"));
	}

    return result;
}

bool fs_move(string_t source, string_t destination)
{
	bool result = false;

	m_scoped_temp
	{
		string16_t src16 = utf16_from_utf8(temp, source);
		string16_t dst16 = utf16_from_utf8(temp, destination);

		result = MoveFileExW(src16.data, dst16.data, MOVEFILE_REPLACE_EXISTING);
		if (!result)  output_last_error(strlit16("fs_move failed"));
	}

    return result;
}

bool fs_copy_directory(string_t source, string_t destination)
{
	bool result = false;

	m_scoped_temp
	{
		string16_t src16 = utf16_from_utf8(temp, source);
		string16_t dst16 = utf16_from_utf8(temp, destination);

		SHFILEOPSTRUCTW s = {
			.wFunc = FO_COPY,
			.pFrom = src16.data,
			.pTo = dst16.data,
			.fFlags = FOF_SILENT|FOF_NOCONFIRMMKDIR|FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_NO_UI,
		};

		result = !!SHFileOperationW(&s);
	}

	return result;
}

static bool wide_strings_equal(wchar_t *a, wchar_t *b)
{
    do
    {
        if (*a != *b)
        {
            return false;
        }
        a++;
        b++;
    }
    while (*a && *b);

    return *a == *b;
}

static fs_entry_t *fs_scan_directory_(arena_t *arena, string_t path, int flags, fs_entry_t *parent_dir)
{
	arena_t *temp = m_get_temp(&arena, 1);
	m_scope_begin(temp);

    string16_t path16 = utf16_from_utf8(temp, string_format(temp, "%.*s\\*", strexpand(path)));

    fs_entry_t *first = NULL;
    fs_entry_t *last  = NULL;

    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(path16.data, &data);
    if (handle != INVALID_HANDLE_VALUE)
    {
        for (;;)
        {
            bool skip = wide_strings_equal(data.cFileName, L".") ||
                        wide_strings_equal(data.cFileName, L"..");

            if (!(flags & FS_SCAN_DONT_SKIP_DOTFILES))
            {
                skip |= data.cFileName[0] == '.';
            }

            if (!skip)
            {
                fs_entry_t *entry = m_alloc_struct(arena, fs_entry_t);

                dll_push_back(first, last, entry);

                entry->parent = parent_dir;

                entry->kind = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FS_ENTRY_DIRECTORY : FS_ENTRY_FILE;
                entry->name = utf8_from_utf16(arena, string16_from_cstr(data.cFileName));

                if (entry->parent)
                {
                    entry->path = string_format(arena, "%.*s/%.*s", strexpand(entry->parent->path), strexpand(entry->name));
                }
                else
                {
                    entry->path = string_format(arena, "%.*s/%.*s", strexpand(path), strexpand(entry->name));
                }

                ULARGE_INTEGER last_write_time = {
                    .LowPart  = data.ftLastWriteTime.dwLowDateTime,
                    .HighPart = data.ftLastWriteTime.dwHighDateTime,
                };

                entry->last_write_time = last_write_time.QuadPart;

                if ((flags & FS_SCAN_RECURSIVE) && entry->kind == FS_ENTRY_DIRECTORY)
                {
                    entry->first_child = fs_scan_directory_(arena, entry->path, flags, entry);
                }
            }

            if (!FindNextFile(handle, &data))
            {
                ASSERT(GetLastError() == ERROR_NO_MORE_FILES);
                break;
            }
        }
    }

	m_scope_end(temp);

    return first;
}

fs_entry_t *fs_scan_directory(arena_t *arena, string_t path, int flags)
{
    return fs_scan_directory_(arena, path, flags, NULL);
}

fs_entry_t *fs_entry_next(fs_entry_t *entry)
{
    fs_entry_t *next = NULL;

    if (entry->first_child)
    {
        next = entry->first_child;
    }
    else
    {
        if (entry->next)
        {
            next = entry->next;
        }
        else
        {
            fs_entry_t *p = entry;
            while (p)
            {
                if (p->next)
                {
                    next = p->next;
                    break;
                }

                p = p->parent;
            }
        }
    }

    return next;
}

fs_create_directory_result_t fs_create_directory(string_t directory)
{
    fs_create_directory_result_t result = CREATE_DIRECTORY_SUCCESS;

	m_scoped_temp
    if (!CreateDirectoryW(utf16_from_utf8(temp, directory).data, NULL))
    {
        switch (GetLastError())
        {
            case ERROR_ALREADY_EXISTS: result = CREATE_DIRECTORY_ALREADY_EXISTS; break;
            case ERROR_PATH_NOT_FOUND: result = CREATE_DIRECTORY_PATH_NOT_FOUND; break;
            INVALID_DEFAULT_CASE;
        }
    }

    return result;
}

fs_create_directory_result_t fs_create_directory_recursive(string_t directory)
{
    fs_create_directory_result_t result = CREATE_DIRECTORY_SUCCESS;

    size_t at = 0;

    while (at < directory.count)
    {
        string_t sub_directory = {0};

        while (at < directory.count)
        {
            if (directory.data[at] == '/' ||
                directory.data[at] == '\\')
            {
                sub_directory = substring(directory, 0, at);

                at += 1;
                break;
            }
            else
            {
                at += 1;
            }
        }

        if (sub_directory.count == 0)
        {
            sub_directory = directory;
        }

        result = fs_create_directory(sub_directory);

        if (result != CREATE_DIRECTORY_SUCCESS &&
            result != CREATE_DIRECTORY_ALREADY_EXISTS)
        {
            break;
        }
    }

    return result;
}

string_t fs_full_path(arena_t *arena, string_t relative_path)
{
    string_t result = strnull;

	m_scoped_temp
	{
		DWORD count = GetFullPathNameW(utf16_from_utf8(temp, relative_path).data, 0, NULL, NULL);
		if (ALWAYS(count > 0))
		{
			wchar_t *data = m_alloc_string16(temp, count);
			GetFullPathNameW(data, count, data, NULL);

			result = utf8_from_utf16(arena, (string16_t) { count-1, data });
		}
	}

    return result;
}

string_t os_get_working_directory(arena_t *arena)
{
    string_t result = strnull;

    DWORD count = GetCurrentDirectoryW(0, NULL);
    if (ALWAYS(count > 0))
    {
		arena_t *temp = m_get_temp(&arena, 1);

		m_scoped(temp)
		{
			wchar_t *data = m_alloc_string16(temp, count);
			GetCurrentDirectory(count, data);

			result = utf8_from_utf16(arena, (string16_t) { count-1, data });
		}
    }

    return result;
}

bool os_set_working_directory(string_t directory)
{
	bool result = false;

	m_scoped_temp
	{
		result = SetCurrentDirectoryW(utf16_from_utf8(temp, directory).data);
		if (!result) output_last_error(strlit16("os_set_working_directory"));
	}

    return result;
}

bool os_execute(string_t command, int *exit_code)
{
	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

    string16_t command16 = utf16_from_utf8(temp, command);

    HANDLE stdout_read, stdout_write;
    HANDLE stderr_read, stderr_write;

    SECURITY_ATTRIBUTES s_attr = {
        .nLength        = sizeof(s_attr),
        .bInheritHandle = TRUE,
    };

    if (!CreatePipe(&stdout_read, &stdout_write, &s_attr, 0))
        return false;

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
        return false;

    if (!CreatePipe(&stderr_read, &stderr_write, &s_attr, 0))
        return false;

    if (!SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0))
        return false;

    STARTUPINFOW startup = {
        .cb = sizeof(startup),
        .hStdOutput = stdout_write,
        .hStdError  = stderr_write,
        .hStdInput  = GetStdHandle(STD_INPUT_HANDLE),
        .dwFlags    = STARTF_USESTDHANDLES,
    };

    PROCESS_INFORMATION info;
    bool result = CreateProcessW(NULL,
                                 (wchar_t *)command16.data,
                                 NULL,
                                 NULL,
                                 TRUE,
                                 0,
                                 NULL,
                                 NULL,
                                 &startup,
                                 &info);

    if (!result)
    {
        output_last_error(strlit16("CreateProcessW failed"));
    }

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (result)
    {
        HANDLE standard_out = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE standard_err = GetStdHandle(STD_ERROR_HANDLE);

        enum { BUFFER_SIZE = 4096 };
        char buffer[BUFFER_SIZE];
        DWORD read, written;
        BOOL success;

        for (;;)
        {
            success = ReadFile(stdout_read, buffer, BUFFER_SIZE, &read, NULL);
            if (!success || read == 0)  break;

            success = WriteFile(standard_out, buffer, read, &written, NULL);
            if (!success)  break;
        }

        for (;;)
        {
            success = ReadFile(stderr_read, buffer, BUFFER_SIZE, &read, NULL);
            if (!success || read == 0)  break;

            success = WriteFile(standard_err, buffer, read, &written, NULL);
            if (!success)  break;
        }
    }

    if (exit_code)
    {
        GetExitCodeProcess(info.hProcess, (DWORD *)exit_code);
    }

    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

	m_scope_end(temp);

    return result;
}

bool os_execute_capture(string_t command, int *exit_code, arena_t *arena, string_t *out, string_t *err)
{
	arena_t *temp = m_get_temp(NULL, 0);
	m_scope_begin(temp);

    string16_t command16 = utf16_from_utf8(temp, command);

    HANDLE stdout_read, stdout_write;
    HANDLE stderr_read, stderr_write;

    SECURITY_ATTRIBUTES s_attr = {
        .nLength        = sizeof(s_attr),
        .bInheritHandle = TRUE,
    };

    if (!CreatePipe(&stdout_read, &stdout_write, &s_attr, 0))
        return false;

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
        return false;

    if (!CreatePipe(&stderr_read, &stderr_write, &s_attr, 0))
        return false;

    if (!SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0))
        return false;

    STARTUPINFOW startup = {
        .cb = sizeof(startup),
        .hStdOutput = stdout_write,
        .hStdError  = stderr_write,
        .hStdInput  = GetStdHandle(STD_INPUT_HANDLE),
        .dwFlags    = STARTF_USESTDHANDLES,
    };

    PROCESS_INFORMATION info;
    bool result = CreateProcessW(NULL,
                                 (wchar_t *)command16.data,
                                 NULL,
                                 NULL,
                                 TRUE,
                                 0,
                                 NULL,
                                 NULL,
                                 &startup,
                                 &info);

    if (!result)
    {
        output_last_error(strlit16("CreateProcessW failed"));
    }

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (result)
    {
        // TODO: something less stupid?
        string_list_t out_list = { 0 };
        string_list_t err_list = { 0 };

        enum { BUFFER_SIZE = 4096 };
        char buffer[BUFFER_SIZE];
        DWORD read;
        BOOL success;

        for (;;)
        {
            success = ReadFile(stdout_read, buffer, BUFFER_SIZE, &read, NULL);
            if (!success || read == 0)  break;

            slist_appends(&out_list, temp, (string_t){read, buffer});
        }

        for (;;)
        {
            success = ReadFile(stderr_read, buffer, BUFFER_SIZE, &read, NULL);
            if (!success || read == 0)  break;

            slist_appends(&err_list, temp, (string_t){read, buffer});
        }

        *out = slist_flatten(&out_list, arena);
        *err = slist_flatten(&err_list, arena);
    }

    if (exit_code)
    {
        GetExitCodeProcess(info.hProcess, (DWORD *)exit_code);
    }

    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

	m_scope_end(temp);

    return result;
}

uint64_t fs_get_last_write_time(string_t path)
{
	ULARGE_INTEGER result = {0};

	m_scoped_temp
	{
		string16_t path_wide = utf16_from_utf8(temp, path);

		FILETIME last_write_time = {0};

		WIN32_FILE_ATTRIBUTE_DATA data;
		if (GetFileAttributesExW(path_wide.data, GetFileExInfoStandard, &data))
		{
			last_write_time = data.ftLastWriteTime;
		}

		result.LowPart  = last_write_time.dwLowDateTime;
		result.HighPart = last_write_time.dwLowDateTime;
	}

    return result.QuadPart;
}

static LARGE_INTEGER qpc_freq;

hires_time_t os_hires_time(void)
{
    LARGE_INTEGER qpc = { 0 };
    QueryPerformanceCounter(&qpc);

    return (hires_time_t) { qpc.QuadPart };
}

double os_seconds_elapsed(hires_time_t start, hires_time_t end)
{
    if (qpc_freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&qpc_freq);
    }

    double result = (double)(end.value - start.value) / (double)qpc_freq.QuadPart;
    return result;
}
