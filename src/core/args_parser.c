fn void init_args(cmd_args_t *args, int argc, char **argv)
{
    args->error = false;

    args->at  = argv + 1;
    args->end = args->at + argc - 1;
}

fn bool args_left(cmd_args_t *args)
{
    return args->at < args->end;
}

fn bool args_match(cmd_args_t *args, const char *arg)
{
    if (strcmp(*args->at, arg) == 0)
    {
        args->at += 1;
        return true;
    }
    return false;
}

fn void args_error(cmd_args_t *args, string_t failure_reason)
{
    fprintf(stderr, "argument parse error: %.*s", strexpand(failure_reason));
    args->error = true;
}

fn string_t args_next(cmd_args_t *args)
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

fn int args_parse_int(cmd_args_t *args)
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

fn float args_parse_float(cmd_args_t *args)
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

fn void skip_arg(cmd_args_t *args)
{
    args->at += 1;
}
