#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

//
//
//

#include "../fs.h"
#include "core/common.h"
#include "core/string.h"
#include "core/arena.h"
#include "core/tls.h"

//
// helper functions (TODO: deduplicate these per platform? unicode plugin?)
//

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
    OutputDebugStringW(string16_null_terminate(temp, prefix));
    OutputDebugStringW(L": ");
    OutputDebugStringW(message);
    LocalFree(message);
}

static void error_box(char *message)
{
    MessageBoxA(NULL, message, "Error", MB_OK);
}

#if 0
static string16_t utf16_from_utf8(arena_t *arena, string_t utf8)
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

static string_t utf8_from_utf16(arena_t *arena, string16_t utf16)
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
#endif

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

//
//
//

static string_t read_entire_file(arena_t *arena, string_t path)
{
    string_t result = { 0 };

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
                char *buffer = m_alloc_string(arena, file_size_low + 1);

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

    return result;
}

static bool write_entire_file(string_t path, string_t file)
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

static bool copy(string_t source, string_t destination)
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

static bool move(string_t source, string_t destination)
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

static bool copy_directory(string_t source, string_t destination)
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

static fs_entry_t *scan_directory_(arena_t *arena, string_t path, int flags, fs_entry_t *parent_dir)
{
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
                zero_struct(entry);

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
                    entry->first_child = scan_directory_(arena, entry->path, flags, entry);
                }
            }

            if (!FindNextFile(handle, &data))
            {
                ASSERT(GetLastError() == ERROR_NO_MORE_FILES);
                break;
            }
        }
    }

    return first;
}

static fs_entry_t *scan_directory(arena_t *arena, string_t path, int flags)
{
    return scan_directory_(arena, path, flags, NULL);
}

static fs_create_directory_result_t create_directory(string_t directory)
{
    fs_create_directory_result_t result = CREATE_DIRECTORY_SUCCESS;

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

static string_t full_path(arena_t *arena, string_t relative_path)
{
    string_t result = strnull;

    DWORD count = GetFullPathNameW(utf16_from_utf8(temp, relative_path).data, 0, NULL, NULL);
    if (ALWAYS(count > 0))
    {
        wchar_t *data = m_alloc_string16(temp, count);
        GetFullPathNameW(data, count, data, NULL);

        result = utf8_from_utf16(arena, (string16_t) { count-1, data });
    }

    return result;
}

static fs_i fs = {
	.read_entire_file  = read_entire_file,
	.write_entire_file = write_entire_file,
	.copy              = copy,
	.move              = move,
	.copy_directory    = copy_directory,
	.scan_directory    = scan_directory,
	.create_directory  = create_directory,
	.full_path         = full_path,
};

DREAM_PLUG_LOAD(fs)
{
	(void)io;
	return &fs;
}

DREAM_PLUG_UNLOAD(fs)
{
	(void)io;
}
