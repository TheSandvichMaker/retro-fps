// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef enum log_level_t
{
	LogLevel_Info    = 1,
	LogLevel_Warning = 7,
	LogLevel_Error   = 10,
} log_level_t;

extern string_t log_level_to_string[];

typedef enum log_category_t
{
	LogCat_None          = 0,

	LogCat_Asset         = 1,
	LogCat_Mixer         = 2,
	LogCat_Renderer      = 3,
	LogCat_Renderer_DX11 = 4,
	LogCat_RHI_D3D12     = 5,
	LogCat_Game          = 10,

	LogCat_Max,
} log_category_t;

extern string_t log_category_to_string[];

typedef struct log_loc_t
{
	const char *file;
	int line;
} log_loc_t;

#define MAKE_LOG_LOCATION &(log_loc_t){ .file = __FILE__, .line = __LINE__ }

#define logs(cat, level, message)     logs_  (MAKE_LOG_LOCATION, LogCat_##cat, LogLevel_##level, message)
#define log(cat, level, fmt, ...)     log_   (MAKE_LOG_LOCATION, LogCat_##cat, LogLevel_##level, fmt, ##__VA_ARGS__)
#define log_va(cat, level, fmt, args) log_va_(MAKE_LOG_LOCATION, LogCat_##cat, LogLevel_##level, fmt, args)
fn void logs_  (const log_loc_t *loc, log_category_t cat, log_level_t level, string_t message);
fn void log_   (const log_loc_t *loc, log_category_t cat, log_level_t level, const char *fmt, ...);
fn void log_va_(const log_loc_t *loc, log_category_t cat, log_level_t level, const char *fmt, va_list args);