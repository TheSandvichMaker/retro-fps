#ifndef ASSERT_H
#define ASSERT_H

#define ASSERT(expr) ((expr) ? true : (fatal_error(__LINE__, strlit(__FILE__), "Assertion failed: " #expr), false))
#define ASSERT_MSG(expr, msg, ...) ((expr) ? true : (fatal_error(__LINE__, strlit(__FILE__), msg, ##__VA_ARGS__), false))

// these can get compiled away so no side effects in the expression!!!
#ifdef NDEBUG
#define DEBUG_ASSERT(...)
#define DEBUG_ASSERT_MSG(...)
#else
#define DEBUG_ASSERT(expr) ASSERT(expr)
#define DEBUG_ASSERT_MSG(expr, msg) ASSERT_MSG(expr, msg)
#endif

#define ALWAYS(expr) ASSERT(expr) // assert behaves like ALWAYS already
#define NEVER(expr) !ALWAYS(!(expr))

#define FATAL_ERROR(msg, ...) ASSERT_MSG(false, msg, ##__VA_ARGS__)

#define INVALID_DEFAULT_CASE default: { FATAL_ERROR("Reached invalid default case!"); } break;

void fatal_error(int line, string_t file, const char *fmt, ...);
void fatal_error_va(int line, string_t file, const char *fmt, va_list args);

#endif /* ASSERT_H */
