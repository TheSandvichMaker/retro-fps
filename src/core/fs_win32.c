// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

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
		if (!result)  win32_output_last_error(strlit16("fs_copy failed"));
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
		if (!result)  win32_output_last_error(strlit16("fs_move failed"));
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

fn_local bool wide_strings_equal(wchar_t *a, wchar_t *b)
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

fn_local fs_entry_t *fs_scan_directory_(arena_t *arena, string_t path, int flags, fs_entry_t *parent_dir)
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

            if (!(flags & FsScanDirectory_dont_skip_dotfiles))
            {
                skip |= data.cFileName[0] == '.';
            }

            if (!skip)
            {
                fs_entry_t *entry = m_alloc_struct(arena, fs_entry_t);

                dll_push_back(first, last, entry);

                entry->parent = parent_dir;

                entry->kind = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FsEntryKind_directory : FsEntryKind_file;
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

                if ((flags & FsScanDirectory_recursive) && entry->kind == FsEntryKind_directory)
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
    fs_create_directory_result_t result = FsCreateDirectory_success;

	m_scoped_temp
    if (!CreateDirectoryW(utf16_from_utf8(temp, directory).data, NULL))
    {
        switch (GetLastError())
        {
            case ERROR_ALREADY_EXISTS: result = FsCreateDirectory_already_exists; break;
            case ERROR_PATH_NOT_FOUND: result = FsCreateDirectory_path_not_found; break;
            INVALID_DEFAULT_CASE;
        }
    }

    return result;
}

fs_create_directory_result_t fs_create_directory_recursive(string_t directory)
{
    fs_create_directory_result_t result = FsCreateDirectory_success;

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

        if (result != FsCreateDirectory_success &&
            result != FsCreateDirectory_already_exists)
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

