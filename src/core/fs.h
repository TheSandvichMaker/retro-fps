// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

fn string_t fs_read_entire_file (arena_t *arena, string_t path);
fn bool     fs_write_entire_file(string_t path, string_t file);

fn bool fs_copy(string_t source, string_t destination);
fn bool fs_move(string_t source, string_t destination);

fn bool fs_copy_directory(string_t source, string_t destination);

typedef enum fs_entry_kind_t
{
    FsEntryKind_file,
    FsEntryKind_directory,
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
    FsScanDirectory_recursive          = 1 << 0,
    FsScanDirectory_dont_skip_dotfiles = 1 << 1,
} fs_scan_directory_flags_t;

fs_entry_t *fs_scan_directory(arena_t *arena, string_t path, int flags);
fs_entry_t *fs_entry_next(fs_entry_t *entry);

typedef enum fs_create_directory_result_t
{
    FsCreateDirectory_success,
    FsCreateDirectory_already_exists,
    FsCreateDirectory_path_not_found,
} fs_create_directory_result_t;

fn fs_create_directory_result_t fs_create_directory(string_t directory);
fn fs_create_directory_result_t fs_create_directory_recursive(string_t directory);

fn string_t fs_full_path(arena_t *arena, string_t relative_path);

fn uint64_t fs_get_last_write_time(string_t path);
