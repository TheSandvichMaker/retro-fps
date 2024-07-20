// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

/* ported from cbloom's robust win32 io */

enum
{
	Win32_io_notaligned          = 1,
	Win32_io_unbuffered_aligment = 4096,
	Win32_io_max_retries         = 10,
	Win32_io_max_single_io_size  = 1 << 24,
};

typedef enum win32_io_status_t
{
	Win32_io_started_async,
	Win32_io_done_sync,
	Win32_io_done_sync_eof,
	Win32_io_error,
} win32_io_status_t;

fn_local win32_io_status_t win32_async_read(HANDLE      handle, 
											uint64_t    offset, 
											uint32_t    size, 
											void       *buffer, 
											OVERLAPPED *overlapped, 
											uint32_t   *read_size)
{
	ASSERT(buffer);
	ASSERT(overlapped);
	ASSERT(read_size);

	zero_struct(overlapped);

	LARGE_INTEGER offset_ = {
		.QuadPart = offset,
	};

	overlapped->Offset     = offset_.LowPart;
	overlapped->OffsetHigh = offset_.HighPart;

	// There is a bug in Windows XP where ReadFile/WriteFile returns an error but doesn't set LastError,
	// this guards against that, in case this code ever runs on XP, I guess...
	SetLastError(0);

	DWORD bytes_read = 0;
	BOOL  read_ok    = ReadFile(handle, buffer, size, &bytes_read, overlapped);

	*read_size = bytes_read;

	win32_io_status_t result = Win32_io_error;

	if (read_ok)
	{
		// weird case, ReadFile returned done immediately - not overlapped? not async!
		zero_struct(overlapped);

		DWORD error = GetLastError();

		if (error == ERROR_HANDLE_EOF)
		{
			result = Win32_io_done_sync_eof;
		}
		else
		{
			result = Win32_io_done_sync;
		}
	}
	else
	{
		DWORD error = GetLastError();

		if (error == ERROR_IO_PENDING)
		{
			// started async read successfully
			result = Win32_io_started_async;
		}
		else if (error == ERROR_HANDLE_EOF)
		{
			// we were at EOF at start of read - not async!
			zero_struct(overlapped);

			result = Win32_io_done_sync_eof;
		}
		else
		{
			// not good error!
			// @LoggingInCore
			// m_scoped_temp
			// {
			// 	string_t error_message = win32_format_error(temp, error);
			// 	log(Filesystem, Error, "Win32 async read error: %.*s", Sx(error_message));
			// }
		}
	}

	return result;
}

fn_local win32_io_status_t win32_async_write(HANDLE      handle,
											 uint64_t    offset,
											 uint32_t    size,
											 const void *buffer,
											 OVERLAPPED *overlapped,
											 uint32_t   *written_size)
{
	ASSERT(buffer);
	ASSERT(overlapped);
	ASSERT(written_size);
	
	zero_struct(overlapped);

	LARGE_INTEGER offset_ = {
		.QuadPart = offset,
	};

	overlapped->Offset     = offset_.LowPart;
	overlapped->OffsetHigh = offset_.HighPart;

	// There is a bug in Windows XP where ReadFile/WriteFile returns an error but doesn't set LastError,
	// this guards against that, in case this code ever runs on XP, I guess...
	SetLastError(0);

	DWORD bytes_written = 0;
	BOOL  write_ok      = WriteFile(handle, buffer, size, &bytes_written, overlapped);

	*written_size = bytes_written;

	win32_io_status_t result = Win32_io_error;

	if (write_ok)
	{
		// WriteFile returned done immediately - not overlapped?

		// Almost all writes go through this path:
		// http://support.microsoft.com/kb/156932

		// for writes to be actually async you must have:
		//   used unbuffered IO
		//   pre-extended the size of the file
		//   validated that range with SetFileValidData
		// and even that is no guarantee

		// not async!
		zero_struct(overlapped);
		
		result = Win32_io_done_sync;
	}
	else
	{
		DWORD error = GetLastError();

		if (error == ERROR_IO_PENDING)
		{
			// good successful async write
			result = Win32_io_started_async;
		}
		else
		{
			// not good error!
			// @LoggingInCore
			// m_scoped_temp
			// {
			// 	string_t error_message = win32_format_error(temp, error);
			// 	log(Filesystem, Error, "Win32 async write error: %.*s", Sx(error_message));
			// }
		}
	}

	return result;
}

fn_local win32_io_status_t win32_get_async_result(HANDLE handle, OVERLAPPED *overlapped, uint32_t *out_size)
{
	ASSERT(overlapped);
	ASSERT(out_size);

	win32_io_status_t result = Win32_io_error;

	DWORD size = 0;

	// first check the result with no wait - this also resets the event so that the next call to GOR works
	if (GetOverlappedResult(handle, overlapped, &size, FALSE))
	{
		if (size > 0)
		{
			result = Win32_io_done_sync;
		}
	}
	else
	{
		// calling GOR with TRUE will yield of thread if the IO is still pending
		if (GetOverlappedResult(handle, overlapped, &size, TRUE))
		{
			result = Win32_io_done_sync;
		}
		else
		{
			DWORD error = GetLastError();

			if (error == ERROR_HANDLE_EOF)
			{
				result = Win32_io_done_sync_eof;
			}
			else
			{
				// not good error!
				// @LoggingInCore
				// m_scoped_temp
				// {
				// 	string_t error_message = win32_format_error(temp, error);
				// 	log(Filesystem, Error, "Win32 get async result error: %.*s", Sx(error_message));
				// }
			}
		}
	}

	*out_size = size;

	return result;
}

fn_local win32_io_status_t win32_sync_read_sub(HANDLE    handle, 
											   uint64_t  offset, 
											   uint32_t  size, 
											   void     *buffer, 
											   uint32_t *read_size)
{
	if (read_size)
	{
		*read_size = 0;
	}

	win32_io_status_t result = Win32_io_error;

	if (size == 0)
	{
		result = Win32_io_done_sync;
	}
	else
	{
		for (size_t retry_index = 0; retry_index < Win32_io_max_retries; retry_index++)
		{
			OVERLAPPED async = { 0 };

			uint32_t          bytes_read = 0;
			win32_io_status_t status     = win32_async_read(handle, offset, size, buffer, &async, &bytes_read);

			if (status == Win32_io_started_async)
			{
				status = win32_get_async_result(handle, &async, &bytes_read);
			}

			if (status == Win32_io_done_sync_eof)
			{
				if (read_size)
				{
					*read_size = bytes_read;
				}

				result = status;
				break;
			}
			else if (status == Win32_io_done_sync)
			{
				if (bytes_read > 0)
				{
					if (read_size)
					{
						*read_size = bytes_read;
					}

					result = status;
					break;
				}

				// else, retry
			}
			else
			{
				ASSERT(status == Win32_io_error);

				DWORD error = GetLastError();

				if (error == ERROR_NO_SYSTEM_RESOURCES ||
					error == ERROR_NOT_ENOUGH_MEMORY)
				{
					DWORD milliseconds = MIN(1 + (DWORD)retry_index*10, 50);
					Sleep(milliseconds);
				}
				else
				{
					// Unexpected error!
					// @LoggingInCore
					// m_scoped_temp
					// {
					// 	string_t error_message = win32_format_error(temp, error);
					// 	log(Filesystem, Error, "Win32 sync read sub error: %.*s", Sx(error_message));
					// }

					result = Win32_io_error;
					break;
				}
			}
		}
	}

	return result;
}

fn_local win32_io_status_t win32_sync_write_sub(HANDLE      handle,
												uint64_t    offset,
												uint32_t    size,
												const void *buffer,
												uint32_t   *written_size)
{
	if (written_size)
	{
		*written_size = 0;
	}

	win32_io_status_t result = Win32_io_error;

	if (size == 0)
	{
		result = Win32_io_done_sync;
	}
	else
	{
		for (size_t retry_index = 0; retry_index < Win32_io_max_retries; retry_index++)
		{
			OVERLAPPED async = { 0 };

			uint32_t          bytes_written = 0;
			win32_io_status_t status        = win32_async_write(handle, offset, size, buffer, &async, &bytes_written);

			if (status == Win32_io_started_async)
			{
				status = win32_get_async_result(handle, &async, &bytes_written);
			}

			if (status == Win32_io_done_sync || status == Win32_io_done_sync_eof)
			{
				if (written_size)
				{
					*written_size = bytes_written;
				}

				result = status;
				break;
			}
			else
			{
				DWORD error = GetLastError();

				if (error == ERROR_NO_SYSTEM_RESOURCES ||
					error == ERROR_NOT_ENOUGH_MEMORY)
				{
					DWORD milliseconds = MIN(1 + (uint32_t)retry_index*10, 50);
					Sleep(milliseconds);
				}
				else
				{
					// Unexpected error!
					// @LoggingInCore
					// m_scoped_temp
					// {
					// 	string_t error_message = win32_format_error(temp, error);
					// 	log(Filesystem, Error, "Win32 sync write sub error: %.*s", Sx(error_message));
					// }

					result = Win32_io_error;
					break;
				}
			}
		}
	}

	return result;
}

fn_local bool win32_sync_read(HANDLE    handle, 
							  uint64_t  offset, 
							  uint64_t  size, 
							  void     *buffer, 
							  uint64_t *bytes_read, 
							  uint32_t  alignment)
{
	if (alignment == 0)
	{
		alignment = 1;
	}

	if ((offset & (alignment - 1)) != 0)
	{
		return false;
	}

	bool result = true;

	char *at = buffer;

	uint64_t total_bytes_read = 0;

	while (size > 0)
	{
		uint32_t chunk_read_size = (uint32_t)MIN(size, Win32_io_max_single_io_size);

		uint32_t          chunk_bytes_read = 0;
		win32_io_status_t status           = win32_sync_read_sub(handle, offset, chunk_read_size, at, &chunk_bytes_read);

		if (status == Win32_io_error)
		{
			result = false;
			break;
		}

		total_bytes_read += chunk_bytes_read;

		if (status == Win32_io_done_sync_eof)
		{
			result = true;
			break;
		}

		if (chunk_bytes_read == 0)
		{
			// successful read, yet we got no bytes. error!
			result = false;
			break;
		}

		offset += chunk_bytes_read;
		at     += chunk_bytes_read;
		size   -= chunk_bytes_read;

		if ((offset & (alignment - 1)) != 0)
		{
			// If we got off the alignment due to a partial IO, it must be due to EOF, so I guess we succeeded.
			result = true;
			break;
		}
	}

	if (bytes_read)
	{
		*bytes_read = total_bytes_read;
	}

	return result;
}

fn_local bool win32_sync_write(HANDLE      handle, 
							   uint64_t    offset, 
							   uint64_t    size, 
							   const void *buffer, 
							   uint64_t   *bytes_written, 
							   uint32_t    alignment)
{
	if (alignment == 0)
	{
		alignment = 1;
	}

	if ((offset & (alignment - 1)) != 0)
	{
		return false;
	}

	bool result = true;

	const char *at = buffer;

	uint64_t total_bytes_written = 0;

	while (size > 0)
	{
		uint32_t chunk_write_size = (uint32_t)MIN(size, Win32_io_max_single_io_size);

		uint32_t          chunk_bytes_written = 0;
		win32_io_status_t status              = win32_sync_write_sub(handle, offset, chunk_write_size, at, &chunk_bytes_written);

		if (status == Win32_io_error)
		{
			result = false;
			break;
		}

		total_bytes_written += chunk_bytes_written;

		offset += chunk_bytes_written;
		at     += chunk_bytes_written;
		size   -= chunk_bytes_written;

		if (status == Win32_io_done_sync_eof)
		{
			// EOF during write? might be possible with read-write file in non-append mode?

			if (size == 0)
			{
				result = true; // wrote all bytes
			}
			else
			{
				result = false; // did not write all bytes :(
			}

			break;
		}

		ASSERT(status == Win32_io_done_sync);

		if (chunk_bytes_written == 0)
		{
			// succeeded, and yet no bytes written? suspicious
			result = false;
			break;
		}

		if ((offset & (alignment - 1)) != 0)
		{
			// If we got off the alignment due to a partial IO, it must be due to EOF, so I guess we succeeded.
			result = true;
			break;
		}
	}

	if (bytes_written)
	{
		*bytes_written = total_bytes_written;
	}

	return result;
}

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

			uint64_t file_size = (LARGE_INTEGER){ .LowPart  = file_size_low, .HighPart = file_size_high, }.QuadPart;

			if (file_size > 0)
			{
				char *buffer = m_alloc_nozero(arena, file_size + 1, 16);

				uint64_t bytes_read;
				bool success = win32_sync_read(handle, 0, file_size, buffer, &bytes_read, 1);

				if (success)
				{
					buffer[bytes_read] = 0;
					result.count = bytes_read;
					result.data  = buffer;
				}
			}
		}

		CloseHandle(handle);
	}

    return result;
}

bool fs_write_entire_file(string_t path, string_t file)
{
    bool result = true;

	// @LoggingInCore

    m_scoped_temp
    {
        string16_t path16 = utf16_from_utf8(temp, path);

        HANDLE handle = CreateFileW(path16.data, 
									GENERIC_WRITE, 
									FILE_SHARE_WRITE, 
									NULL, 
									CREATE_ALWAYS, 
									FILE_ATTRIBUTE_NORMAL, 
									NULL);

        if (handle != INVALID_HANDLE_VALUE)
        {
			uint64_t written;
			result = win32_sync_write(handle, 0, file.count, file.data, &written, 1);

			CloseHandle(handle);
        }
        else
        {
            result = false;
        }
    }

    return result;
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

    string16_t path16 = utf16_from_utf8(temp, string_format(temp, "%cs\\*", path));

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
                    entry->path = string_format(arena, "%cs/%cs", entry->parent->path, entry->name);
                }
                else
                {
                    entry->path = string_format(arena, "%cs/%cs", path, entry->name);
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
    string_t result = {0};

	m_scoped_temp
	{
		DWORD count = GetFullPathNameW(utf16_from_utf8(temp, relative_path).data, 0, NULL, NULL);
		if (ALWAYS(count > 0))
		{
			wchar_t *data = m_alloc_string16(temp, count);
			GetFullPathNameW(data, count, data, NULL);

			result = utf8_from_utf16(arena, (string16_t) { data, count-1 });
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

