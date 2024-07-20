// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

wchar_t *win32_format_error_wide(HRESULT error)
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

string_t win32_format_error(arena_t *arena, HRESULT error)
{
	wchar_t *message = win32_format_error_wide(error);
	string_t result = utf8_from_utf16(arena, string16_from_cstr(message));
	LocalFree(message);
	return result;
}

void win32_display_last_error(void)
{
    DWORD error = GetLastError() & 0xFFFF;
    wchar_t *message = win32_format_error_wide(error);
    MessageBoxW(0, message, L"Error", MB_OK);
    LocalFree(message);
    __debugbreak();
}

void win32_output_last_error(string16_t prefix)
{
	wchar_t *message = win32_format_error_wide(GetLastError());

	m_scoped_temp
	OutputDebugStringW(string16_null_terminate(temp, prefix).data);

	OutputDebugStringW(L": ");
	OutputDebugStringW(message);
	LocalFree(message);
}

void win32_error_box(char *message)
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

void loud_error(int line, string_t file, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    loud_error_va(line, file, fmt, args);
    va_end(args);
}

void loud_error_va(int line, string_t file, const char *fmt, va_list args)
{
    char buffer[4096];

    string_t message   = string_format_into_buffer_va(buffer, sizeof(buffer), fmt, args);
    string_t formatted = string_format_into_buffer(buffer + message.count, sizeof(buffer) - message.count, 
                                                   "Error: %cs\nLine: %d\nFile: %cs\n", message, line, file);

    MessageBoxA(NULL, formatted.data, "Fatal Error", MB_OK);
	DEBUG_BREAK();
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

#if DREAM_DEVELOPER
    string_t formatted = string_format_into_buffer(buffer + message.count, sizeof(buffer) - message.count, 
                                                   "Fatal error: %cs\nLine: %d\nFile: %cs\n\n'Abort' to exit process, 'Retry' to break in debugger, 'Ignore' to ignore the error and continue regardless.", message, line, file);

    int result = MessageBoxA(NULL, formatted.data, "Fatal Error", MB_ABORTRETRYIGNORE|MB_DEFBUTTON1|MB_SYSTEMMODAL);

	if (result == IDABORT)
	{
		DEBUG_BREAK();
		FatalExit(1);
	}
	else if (result == IDRETRY)
	{
		DEBUG_BREAK();
	}
	else
	{
		/* do nothing... */
	}
#else
    string_t formatted = string_format_into_buffer(buffer + message.count, sizeof(buffer) - message.count, 
                                                   "Fatal error: %cs\nLine: %d\nFile: %cs\n", message, line, file);

    MessageBoxA(NULL, formatted.data, "Fatal Error", MB_OK);
	FatalExit(1);
#endif
}

void win32_hresult_error_box(HRESULT hr, const char *message, ...)
{
	va_list args;
	va_start(args, message);
	win32_hresult_error_box_va(hr, message, args);
	va_end(args);
}

void win32_hresult_error_box_va(HRESULT hr, const char *message_fmt, va_list args)
{
	m_scoped_temp
	{
		string_t message = string_format_va  (temp, message_fmt, args);
		string_t error   = win32_format_error(temp, hr);

		string_t final = string_format(temp, "%cs.\nHRESULT: 0x%X\nExplanation: %cs", message, hr, error);
		MessageBoxA(NULL, final.data, "Win32 Error", MB_OK);
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
        win32_output_last_error(strlit16("vm_commit failed"));

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
	m_scoped_temp
	{
		string_t string = string_format_va(temp, fmt, args);
		string16_t string_wide = utf16_from_utf8(temp, string);
		OutputDebugStringW(string_wide.data);
	}
}

void debug_print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    debug_print_va(fmt, args);

    va_end(args);
}

string_t os_get_working_directory(arena_t *arena)
{
    string_t result = {0};

    DWORD count = GetCurrentDirectoryW(0, NULL);
    if (ALWAYS(count > 0))
    {
		arena_t *temp = m_get_temp(&arena, 1);

		m_scoped(temp)
		{
			wchar_t *data = m_alloc_string16(temp, count);
			GetCurrentDirectory(count, data);

			result = utf8_from_utf16(arena, (string16_t) { data, count-1 });
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
		if (!result) win32_output_last_error(strlit16("os_set_working_directory"));
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
        win32_output_last_error(strlit16("CreateProcessW failed"));
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
        win32_output_last_error(strlit16("CreateProcessW failed"));
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
        DWORD bytes_read;
        BOOL success;

        for (;;)
        {
            success = ReadFile(stdout_read, buffer, BUFFER_SIZE, &bytes_read, NULL);
            if (!success || bytes_read == 0)  break;

            slist_appends(&out_list, temp, (string_t){buffer, bytes_read});
        }

        for (;;)
        {
            success = ReadFile(stderr_read, buffer, BUFFER_SIZE, &bytes_read, NULL);
            if (!success || bytes_read == 0)  break;

            slist_appends(&err_list, temp, (string_t){buffer, bytes_read});
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

static LARGE_INTEGER g_qpc_freq;

hires_time_t os_hires_time(void)
{
    LARGE_INTEGER qpc = { 0 };
    QueryPerformanceCounter(&qpc);

    return (hires_time_t) { qpc.QuadPart };
}

double os_seconds_elapsed(hires_time_t start, hires_time_t end)
{
    if (g_qpc_freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&g_qpc_freq);
    }

    double result = (double)(end.value - start.value) / (double)g_qpc_freq.QuadPart;
    return result;
}

uint64_t os_estimate_cpu_timer_frequency(uint64_t wait_ms)
{
    if (g_qpc_freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&g_qpc_freq);
    }

	uint64_t os_freq      = g_qpc_freq.QuadPart;
	uint64_t os_wait_time = (os_freq * wait_ms) / 1000;
	uint64_t cpu_start    = read_cpu_timer();
	uint64_t os_start     = os_hires_time().value;
	uint64_t os_end       = 0;
	uint64_t os_elapsed   = 0;
	while (os_elapsed < os_wait_time)
	{
		os_end     = os_hires_time().value;
		os_elapsed = os_end - os_start;
	}
	uint64_t cpu_end     = read_cpu_timer();
	uint64_t cpu_elapsed = cpu_end - cpu_start;
	uint64_t cpu_freq    = 0;
	if (os_elapsed)
	{
		cpu_freq = (os_freq * cpu_elapsed) / os_elapsed;
	}
	return cpu_freq;
}

void os_sleep(float milliseconds)
{
	if (milliseconds < 0.0f) return;
	Sleep((DWORD)milliseconds);
}