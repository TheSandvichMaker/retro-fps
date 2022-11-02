#ifndef LOG_H
#define LOG_H

#include "api_types.h"

void debug_print   (const char *fmt, ...);
void debug_print_va(const char *fmt, va_list args);

#endif /* LOG_H */
