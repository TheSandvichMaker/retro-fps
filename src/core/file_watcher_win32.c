#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "file_watcher.h"

// Somewhat shamelessly copied from the Jai File_Watcher module!

typedef struct file_watcher_os_t
{
	DWORD buffer_size;
} file_watcher_os_t;

typedef struct file_watcher_directory_os_t
{
	HANDLE     handle;
	OVERLAPPED overlapped;
	bool       read_pending;
	void      *buffer;
} file_watcher_directory_os_t;

void file_watcher_init(file_watcher_t *watcher)
{
	watcher->os = m_alloc_struct(&watcher->arena, file_watcher_os_t);
	watcher->os->buffer_size = KB(256);
}

void file_watcher_release(file_watcher_t *watcher)
{
	for (file_watcher_directory_t *dir = watcher->first_directory;
		 dir;
		 dir = dir->next)
	{
		CloseHandle(dir->os->handle);
		CloseHandle(dir->os->overlapped.hEvent);
	}

	m_release(&watcher->arena);
}

DREAM_INLINE void file_watcher__issue_read(file_watcher_t *watcher, file_watcher_directory_t *dir)
{
	bool result = ReadDirectoryChangesW(dir->os->handle,
										dir->os->buffer,
										watcher->os->buffer_size,
										TRUE,
										FILE_NOTIFY_CHANGE_CREATION|
										FILE_NOTIFY_CHANGE_FILE_NAME|
										FILE_NOTIFY_CHANGE_LAST_WRITE|
										FILE_NOTIFY_CHANGE_SIZE,
										NULL,
										&dir->os->overlapped,
										NULL);
	dir->os->read_pending = result;

	if (!result)
	{
		debug_print("ReadDirectoryChanges failed for some reason\n");
	}
}

void file_watcher_add_directory(file_watcher_t *watcher, string_t directory)
{
	HANDLE handle = NULL;

	m_scoped_temp
	{
		string16_t wide = utf16_from_utf8(temp, directory);
		handle = CreateFileW(wide.data, 
							 FILE_LIST_DIRECTORY, 
							 FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,
							 NULL,
							 OPEN_EXISTING,
							 FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,
							 NULL);
	}

	if (handle == INVALID_HANDLE_VALUE)
	{
		// TODO: Log error better
		debug_print("Failed to open directory for watching\n");
		return;
	}

	HANDLE event = CreateEventW(NULL, FALSE, FALSE, NULL);

	if (event == NULL)
	{
		// TODO: Log error better
		debug_print("Failed to create event for directory watching\n");
		CloseHandle(handle);
		return;
	}

	file_watcher_directory_t *dir = m_alloc_struct(&watcher->arena, file_watcher_directory_t);
	dir->os                       = m_alloc_struct(&watcher->arena, file_watcher_directory_os_t);

	dir->path = string_copy(&watcher->arena, directory);

	dir->os->handle            = handle;
	dir->os->overlapped.hEvent = event;
	dir->os->buffer            = m_alloc_nozero(&watcher->arena, watcher->os->buffer_size, 16);

	dir->next = watcher->first_directory;
	watcher->first_directory = dir;

	file_watcher__issue_read(watcher, dir);
}

DREAM_INLINE FILE_NOTIFY_INFORMATION *file_watcher__next_notif(FILE_NOTIFY_INFORMATION *notif)
{
	if (notif->NextEntryOffset)
	{
		return (FILE_NOTIFY_INFORMATION *)(((char *)notif) + notif->NextEntryOffset);
	}

	return NULL;
}

file_event_t *file_watcher_get_events(file_watcher_t *watcher, arena_t *arena)
{
	file_event_t *head_event = NULL;
	file_event_t *tail_event = NULL;

	for (file_watcher_directory_t *dir = watcher->first_directory;
		 dir;
		 dir = dir->next)
	{
		if (!dir->os->read_pending)
		{
			continue;
		}

		if (!HasOverlappedIoCompleted(&dir->os->overlapped))
		{
			continue;
		}

		DWORD bytes_written;
		bool result = GetOverlappedResult(dir->os->handle, &dir->os->overlapped, &bytes_written, FALSE);

		if (!result)
		{
			file_watcher__issue_read(watcher, dir);
			continue;
		}

		if (bytes_written == 0)
		{
			debug_print("File watcher overflow!");
			file_watcher__issue_read(watcher, dir);
			continue;
		}

		FILE_NOTIFY_INFORMATION *notifies = (FILE_NOTIFY_INFORMATION *)dir->os->buffer;

		size_t event_count = 0;
		for (FILE_NOTIFY_INFORMATION *notif = notifies; 
			 notif->NextEntryOffset != 0; 
			 notif = file_watcher__next_notif(notif))
		{
			event_count += 1;
		}

		for (FILE_NOTIFY_INFORMATION *notif = notifies; 
			 notif->NextEntryOffset != 0; 
			 notif = file_watcher__next_notif(notif))
		{
			uint32_t flags = 0;

			if (notif->Action == FILE_ACTION_ADDED)            flags |= FileEvent_Added;
			if (notif->Action == FILE_ACTION_REMOVED)          flags |= FileEvent_Removed;
			if (notif->Action == FILE_ACTION_MODIFIED)         flags |= FileEvent_Modified;
			if (notif->Action == FILE_ACTION_RENAMED_OLD_NAME) flags |= FileEvent_Renamed|FileEvent_Renamed_OldName;
			if (notif->Action == FILE_ACTION_RENAMED_NEW_NAME) flags |= FileEvent_Renamed|FileEvent_Renamed_NewName;

			file_event_t *event = m_alloc_struct(arena, file_event_t);

			string16_t name_wide = {
				.data  = &notif->FileName[0],
				.count = notif->FileNameLength / 2,
			};

			event->name      = utf8_from_utf16(arena, name_wide);
			event->path      = string_format(arena, "%.*s/%.*s", Sx(dir->path), Sx(event->name));
			event->flags     = flags;

			sll_push_back(head_event, tail_event, event);
		}

		file_watcher__issue_read(watcher, dir);
	}

	return head_event;
}
