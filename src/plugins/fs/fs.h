#ifndef PLUG_FS_H
#define PLUG_FS_H

#include "core/plug.h"

//
// API
//

#ifndef FILESYSTEM_H

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

typedef enum fs_create_directory_result_t
{
    CREATE_DIRECTORY_SUCCESS,
    CREATE_DIRECTORY_ALREADY_EXISTS,
    CREATE_DIRECTORY_PATH_NOT_FOUND,
} fs_create_directory_result_t;

#endif

typedef struct fs_i
{
	string_t (*read_entire_file) (arena_t *arena, string_t path);
	bool     (*write_entire_file)(string_t path, string_t file);

	bool (*copy)(string_t source, string_t destination);
	bool (*move)(string_t source, string_t destination);

	bool (*copy_directory)(string_t source, string_t destination);

	fs_entry_t *(*scan_directory)(arena_t *arena, string_t path, int flags);

	fs_create_directory_result_t (*create_directory)(string_t directory);

	string_t (*full_path)(arena_t *arena, string_t relative_path);
} fs_i;

//
// inline functions
//

#ifndef FILESYSTEM_H

DREAM_INLINE fs_entry_t *fs_entry_next(fs_entry_t *entry)
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

#endif

//
// plug load/unload
//

DREAM_DECLARE_PLUG(fs);

#endif /* PLUG_FS_H */
