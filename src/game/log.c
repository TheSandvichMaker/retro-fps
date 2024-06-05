// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

string_t log_level_to_string[] = {
	[LogLevel_Spam]    = Sc("Spam"),
	[LogLevel_Info]    = Sc("Info"),
	[LogLevel_Warning] = Sc("Warning"),
	[LogLevel_Error]   = Sc("Error"),
};

string_t log_category_to_string[] = {

	[LogCat_None]          = Sc("None"),
	[LogCat_ActionSystem]  = Sc("ActionSystem"),
	[LogCat_Asset]         = Sc("Asset"),
	[LogCat_Mixer]         = Sc("Mixer"),
	[LogCat_Renderer]      = Sc("Renderer"),
	[LogCat_Renderer_DX11] = Sc("Renderer_DX11"),
	[LogCat_UI]            = Sc("UI"),
	[LogCat_RHI_D3D12]     = Sc("RHI_D3D12"),
	[LogCat_AssetPacker]   = Sc("AssetPacker"),
	[LogCat_MapLoad]       = Sc("MapLoad"),
	[LogCat_Game]          = Sc("Game"),
	[LogCat_Max]           = Sc("INVALID LOG CATEGORY"),
};

typedef struct log_context_t
{
	int dummy;
} log_context_t;

global thread_local log_context_t log_ctx;

void logs_(const log_loc_t *loc, log_category_t cat, log_level_t level, string_t message)
{
	// for the time being, this is not a good / smart logging system

	(void)loc;

	string_t cat_string   = log_category_to_string[cat];
	string_t level_string = log_level_to_string[level];

	debug_print("[%.*s|%.*s] %.*s\n", Sx(cat_string), Sx(level_string), Sx(message));
}

void log_(const log_loc_t *loc, log_category_t cat, log_level_t level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	log_va_(loc, cat, level, fmt, args);

	va_end(args);
}

void log_va_(const log_loc_t *loc, log_category_t cat, log_level_t level, const char *fmt, va_list args)
{
	m_scoped_temp
	{
		string_t message = string_format_va(temp, fmt, args);
		logs_(loc, cat, level, message);
	}
}
