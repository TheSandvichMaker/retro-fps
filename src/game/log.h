// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

typedef enum log_level_t
{
	LogLevel_None,
	LogLevel_SuperSpam,
	LogLevel_Spam,
	LogLevel_Info,
	LogLevel_Warning,
	LogLevel_ValidationFailure,
	LogLevel_Error,
	LogLevel_COUNT,
} log_level_t;

extern string_t log_level_to_string[];

typedef enum log_category_t
{
	LogCat_None          = 0,

	LogCat_Misc,
	LogCat_ActionSystem,
	LogCat_Asset,
	LogCat_Mixer,
	LogCat_Renderer,
	LogCat_Renderer_DX11,
	LogCat_UI,
	LogCat_RHI_D3D12,
	LogCat_AssetPacker,
	LogCat_MapLoad,
	LogCat_Game,

	LogCat_Max,
} log_category_t;

extern string_t log_category_to_string[];

typedef struct log_loc_t
{
	const char *file;
	int line;
} log_loc_t;

#define MAKE_LOG_LOCATION &(log_loc_t){ .file = __FILE__, .line = __LINE__ }

fn void set_log_filter_for_thread(log_level_t level, bool enabled);

#define logs(cat, level, message)     logs_  (MAKE_LOG_LOCATION, LogCat_##cat, LogLevel_##level, message)
#define log(cat, level, fmt, ...)     log_   (MAKE_LOG_LOCATION, LogCat_##cat, LogLevel_##level, fmt, ##__VA_ARGS__)
#define log_va(cat, level, fmt, args) log_va_(MAKE_LOG_LOCATION, LogCat_##cat, LogLevel_##level, fmt, args)
fn void logs_  (const log_loc_t *loc, log_category_t cat, log_level_t level, string_t message);
fn void log_   (const log_loc_t *loc, log_category_t cat, log_level_t level, const char *fmt, ...);
fn void log_va_(const log_loc_t *loc, log_category_t cat, log_level_t level, const char *fmt, va_list args);

#if DREAM_SLOW
#define LOGS_SLOW(cat, level, message)     logs  (cat, level, message)
#define LOG_SLOW(cat, level, fmt, ...)     log   (cat, level, fmt, ##__VA_ARGS__)
#define LOG_VA_SLOW(cat, level, fmt, args) log_va(cat, level, fmt, args)
#else
#define logs_slow(...)     
#define log_slow(...)     
#define log_va_slow(...) 
#endif
