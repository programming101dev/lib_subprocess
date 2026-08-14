#ifndef P101_SUBPROCESS_TOOL_RUN_H
#define P101_SUBPROCESS_TOOL_RUN_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

typedef void (*p101_tool_run_child_setup)(const struct p101_env *env, struct p101_error *err, void *context);

struct p101_tool_run_options
{
    const char               *stdout_path;
    const char               *stderr_path;
    const char               *diagnostic_name;
    mode_t                    output_mode;
    p101_tool_run_child_setup child_setup;
    void                     *child_setup_context;
};

struct p101_tool_argv
{
    char **values;
    size_t count;
    size_t capacity;
};

struct p101_tool_read_pipe
{
    FILE *stream;
    pid_t pid;
};

void p101_tool_argv_init(struct p101_tool_argv *arguments);
bool p101_tool_argv_append(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *arguments, const char *value);
bool p101_tool_argv_append_prefixed(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *arguments, const char *prefix, const char *value);
void p101_tool_argv_destroy(const struct p101_env *env, struct p101_tool_argv *arguments);

int  p101_tool_run_capture(const struct p101_env *env, struct p101_error *err, char *const argv[], const struct p101_tool_run_options *options);
void p101_tool_run_redirect(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path, mode_t output_mode);
bool p101_tool_read_pipe_open(const struct p101_env *env, struct p101_error *err, char *const argv[], const char *diagnostic_name, bool merge_stderr, struct p101_tool_read_pipe *pipe_state);
int  p101_tool_read_pipe_close(const struct p101_env *env, struct p101_error *err, struct p101_tool_read_pipe *pipe_state);

#endif    // P101_SUBPROCESS_TOOL_RUN_H
