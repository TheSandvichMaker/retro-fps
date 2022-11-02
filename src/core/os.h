#ifndef OS_H
#define OS_H

#include "api_types.h"

string_t os_get_working_directory(arena_t *arena);
bool os_set_working_directory(string_t directory);

#define push_working_directory(x) \
    for (string_t __wd = os_get_working_directory(temp); __wd.count && os_set_working_directory(x); (os_set_working_directory(__wd), __wd.count = 0))

bool os_execute(string_t command, int *exit_code);
bool os_execute_capture(string_t command, int *exit_code, arena_t *arena, string_t *out, string_t *err);
// TODO: async os_execute, ability to capture stderr/stdout, provide stdin

typedef struct hires_time_t
{
    uint64_t value;
} hires_time_t;

hires_time_t os_hires_time(void);
double os_seconds_elapsed(hires_time_t start, hires_time_t end);

#endif /* OS_H */
