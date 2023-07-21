#ifndef DREAM_ARGS_PARSER_H
#define DREAM_ARGS_PARSER_H

#include "core/api_types.h"

typedef struct cmd_args_t
{
    char **at;
    char **end;

    bool error;
} cmd_args_t;

DREAM_INLINE void init_args(cmd_args_t *args, int argc, char **argv)
{
    args->error = false;

    args->at  = argv + 1;
    args->end = args->at + argc - 1;
}

DREAM_INLINE bool args_left(cmd_args_t *args)
{
    return args->at < args->end;
}

DREAM_INLINE bool args_match(cmd_args_t *args, const char *arg)
{
    if (strcmp(*args->at, arg) == 0)
    {
        args->at += 1;
        return true;
    }
    return false;
}

DREAM_INLINE void args_error(cmd_args_t *args, string_t failure_reason)
{
    fprintf(stderr, "argument parse error: %.*s", strexpand(failure_reason));
    args->error = true;
}

DREAM_INLINE string_t args_next(cmd_args_t *args)
{
    if (args_left(args))
    {
        string_t result = string_from_cstr(*args->at);
        args->at += 1;
        return result;
    }
    else
    {
        args_error(args, S("expected another argument"));
        return strnull;
    }
}

DREAM_INLINE int args_parse_int(cmd_args_t *args)
{
    string_t arg = args_next(args);

    char *end;
    int result = (int)strtol(arg.data, &end, 0);

    if (end == arg.data)
    {
        args_error(args, S("argument must be an integer"));
    }

    return result;
}

DREAM_INLINE float args_parse_float(cmd_args_t *args)
{
    string_t arg = args_next(args);

    char *end;
    float result = strtof(arg.data, &end);

    if (end == arg.data)
    {
        args_error(args, S("argument must be an integer"));
    }

    return result;
}

DREAM_INLINE void skip_arg(cmd_args_t *args)
{
    args->at += 1;
}

#endif
