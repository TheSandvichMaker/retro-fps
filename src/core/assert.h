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

#define ARRAY_AT(array, index) (*(ASSERT((index) >= 0 && ((index) < ARRAY_COUNT(array))), &array[index]))
#define ARRAY_AT_N(array, index, n) (*(ASSERT((index) >= 0 && ((index) < (n))), &array[index]))

#define DEBUG_BREAK() __debugbreak()

typedef struct debug_break_state_t
{
	bool triggered;
} debug_break_state_t;

// TODO: This could have the debug break register itself to some global table
// so that the triggered state of them could be reset / inhibited through UI.
#define DEBUG_BREAK_ONCE()                                            \
	do                                                                \
	{                                                                 \
		static debug_break_state_t MACRO_IDENT(debug_break_state);    \
		if (!MACRO_IDENT(debug_break_state).triggered)                \
		{                                                             \
			DEBUG_BREAK();                                            \
			MACRO_IDENT(debug_break_state).triggered = true;          \
		}                                                             \
	} while (false)

#define VERIFY(expr) if (!(expr)) { DEBUG_BREAK(); }
#define VERIFY_ONCE(expr) if (!(expr)) { DEBUG_BREAK_ONCE(NULL); }