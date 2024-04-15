// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#include "core.h"

#include "arena.c"
#include "args_parser.c"

#include "hashtable.c"
#include "math.c"
#include "pool.c"
#include "sort.c"
#include "stretchy_buffer.c"
#include "string.c"
#include "string_list.c"
#include "thread.c"
#include "tls.c"
#include "utility.c"

#if PLATFORM_WIN32
#include "core_win32.h"

#include "file_watcher_win32.c"
#include "fs_win32.c"
#include "os_win32.c"
#include "thread_win32.c"
#endif
