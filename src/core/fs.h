#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "api_types.h"

string_t fs_read_entire_file (arena_t *arena, string_t path);
bool     fs_write_entire_file(string_t path, string_t file);

bool fs_move(string_t source, string_t destination);

typedef enum fs_entry_kind_t
{
    FS_ENTRY_FILE,
    FS_ENTRY_DIRECTORY,
} fs_entry_kind_t;

typedef struct fs_entry_t
{
    struct fs_entry_t *next;
    struct fs_entry_t *prev;
    struct fs_entry_t *parent;
    struct fs_entry_t *first_child;

    fs_entry_kind_t kind;

    uint64_t last_write_time;

    size_t file_size;
    string_t name;
    string_t path;
    string_t absolute_path;
} fs_entry_t;

typedef enum fs_scan_directory_flags_t
{
    FS_SCAN_RECURSIVE          = 1 << 0,
    FS_SCAN_DONT_SKIP_DOTFILES = 1 << 1,
} fs_scan_directory_flags_t;

fs_entry_t *fs_scan_directory(arena_t *arena, string_t path, int flags);
fs_entry_t *fs_entry_next(fs_entry_t *entry);

typedef enum fs_create_directory_result_t
{
    CREATE_DIRECTORY_SUCCESS,
    CREATE_DIRECTORY_ALREADY_EXISTS,
    CREATE_DIRECTORY_PATH_NOT_FOUND,
} fs_create_directory_result_t;

fs_create_directory_result_t fs_create_directory(string_t directory);

string_t fs_full_path(arena_t *arena, string_t relative_path);

#endif /* FILESYSTEM_H */
