// ============================================================
// Copyright 2024 by Daniël Cornelisse, All Rights Reserved.
// ============================================================

#pragma once

#define ASSERT_MSG(expr, msg, ...) ((expr) ? true : (fatal_error(__LINE__, S(__FILE__), msg, ##__VA_ARGS__), false))
#define ASSERT(expr) ASSERT_MSG(expr, "Assertion failed: " STRINGIFY(expr))

#define STATIC_ASSERT(expr, ...) _Static_assert(expr, ##__VA_ARGS__)

// these can get compiled away so no side effects in the expression!!!
#ifdef NDEBUG
#define DEBUG_ASSERT(...)
#define DEBUG_ASSERT_MSG(...)
#else
#define DEBUG_ASSERT(expr) ASSERT(expr)
#define DEBUG_ASSERT_MSG(expr, msg) ASSERT_MSG(expr, "(dbg) " msg)
#endif

#define ALWAYS(expr) ASSERT(expr) // assert behaves like ALWAYS already
#define NEVER(expr) !ALWAYS(!(expr))

#define FATAL_ERROR(msg, ...) ASSERT_MSG(false, msg, ##__VA_ARGS__)
#define LOUD_ERROR(msg, ...) loud_error(__LINE__, S(__FILE__), msg, ##__VA_ARGS__)

#define INVALID_DEFAULT_CASE default: { FATAL_ERROR("Reached invalid default case!"); } break;
#define INVALID_CODE_PATH FATAL_ERROR("Invalid code path!");

void loud_error(int line, string_t file, const char *fmt, ...);
void loud_error_va(int line, string_t file, const char *fmt, va_list args);
void fatal_error(int line, string_t file, const char *fmt, ...);
void fatal_error_va(int line, string_t file, const char *fmt, va_list args);
