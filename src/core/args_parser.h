#pragma once

typedef struct cmd_args_t
{
    char **at;
    char **end;

    bool error;
} cmd_args_t;

fn void     init_args       (cmd_args_t *args, int argc, char **argv);
fn bool     args_left       (cmd_args_t *args);
fn bool     args_match      (cmd_args_t *args, const char *arg);
fn void     args_error      (cmd_args_t *args, string_t failure_reason);
fn string_t args_next       (cmd_args_t *args);
fn int      args_parse_int  (cmd_args_t *args);
fn float    args_parse_float(cmd_args_t *args);
fn void     skip_arg        (cmd_args_t *args);
