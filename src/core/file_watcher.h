#ifndef CORE_FILE_WATCHER_H
#define CORE_FILE_WATCHER_H

#include "core/api_types.h"

typedef struct file_watcher_directory_os_t file_watcher_directory_os_t;

typedef struct file_watcher_directory_t
{
	struct file_watcher_directory_t *next;

	string_t path;

	file_watcher_directory_os_t *os;
} file_watcher_directory_t;

typedef struct file_watcher_os_t file_watcher_os_t;

typedef struct file_watcher_t
{
	arena_t arena;

	file_watcher_directory_t *first_directory;

	hires_time_t time_since_last_event;
	struct file_event_t *head_event;
	struct file_event_t *tail_event;
	struct file_event_t *first_free_event;

	file_watcher_os_t *os;
} file_watcher_t;

typedef enum file_event_flags_t
{
	FileEvent_Added           = 1 << 0,
	FileEvent_Removed         = 1 << 1,
	FileEvent_Modified        = 1 << 2,
	FileEvent_Renamed         = 1 << 3,
	FileEvent_Renamed_OldName = 1 << 4,
	FileEvent_Renamed_NewName = 1 << 5,
} file_event_flags_t;

typedef struct file_event_t
{
	struct file_event_t *next;

	string_t name;
	string_t path;
	file_event_flags_t flags;
} file_event_t;

DREAM_GLOBAL void file_watcher_init(file_watcher_t *watcher);
DREAM_GLOBAL void file_watcher_release(file_watcher_t *watcher);

DREAM_GLOBAL void file_watcher_add_directory(file_watcher_t *watcher, string_t directory);
DREAM_GLOBAL file_event_t *file_watcher_get_events(file_watcher_t *watcher, arena_t *arena, double debounce_time);

#endif
