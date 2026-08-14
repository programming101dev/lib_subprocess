#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_subprocess/tool_run.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static int failures;

#define EXPECT(condition)                                                                                                                                                                                                                                          \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        if(!(condition))                                                                                                                                                                                                                                           \
        {                                                                                                                                                                                                                                                          \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                                                                                                                                                             \
            failures++;                                                                                                                                                                                                                                            \
        }                                                                                                                                                                                                                                                          \
    } while(0)

static void test_tool_run(const struct p101_env *env, struct p101_error *err)
{
    struct p101_tool_argv        arguments;
    struct p101_tool_read_pipe   pipe_state;
    struct p101_tool_run_options options = {0};
    char                         line[64];
    char                        *echo_argv[]    = {"/bin/echo", "hello", NULL};
    char                        *success_argv[] = {"/usr/bin/true", NULL};
    char                        *missing_argv[] = {"/definitely/missing/p101-tool", NULL};
    char                        *read_result;
    bool                         bool_result;
    int                          comparison;
    int                          status;

    options.stdout_path     = "/dev/null";
    options.stderr_path     = "/dev/null";
    options.diagnostic_name = "test tool";
    options.output_mode     = 0600;

    status      = p101_tool_run_capture(env, err, success_argv, &options);
    bool_result = p101_error_has_no_error(err);
    EXPECT(bool_result);
    EXPECT(WIFEXITED(status));
    EXPECT(WEXITSTATUS(status) == 0);

    status      = p101_tool_run_capture(env, err, missing_argv, &options);
    bool_result = p101_error_has_no_error(err);
    EXPECT(bool_result);
    EXPECT(WIFEXITED(status));
    EXPECT(WEXITSTATUS(status) == 127);

    p101_tool_argv_init(&arguments);
    bool_result = p101_tool_argv_append(env, err, &arguments, "/bin/echo");
    EXPECT(bool_result);
    bool_result = p101_tool_argv_append_prefixed(env, err, &arguments, "--name=", "p101");
    EXPECT(bool_result);
    EXPECT(arguments.count == 2U);
    comparison = strcmp(arguments.values[0], "/bin/echo");
    EXPECT(comparison == 0);
    comparison = strcmp(arguments.values[1], "--name=p101");
    EXPECT(comparison == 0);
    EXPECT(arguments.values[2] == NULL);
    p101_tool_argv_destroy(env, &arguments);
    EXPECT(arguments.values == NULL);
    EXPECT(arguments.count == 0U);

    bool_result = p101_tool_read_pipe_open(env, err, echo_argv, "test pipe", true, &pipe_state);
    EXPECT(bool_result);
    read_result = fgets(line, sizeof(line), pipe_state.stream);
    EXPECT(read_result != NULL);
    comparison = strcmp(line, "hello\n");
    EXPECT(comparison == 0);
    status      = p101_tool_read_pipe_close(env, err, &pipe_state);
    bool_result = p101_error_has_no_error(err);
    EXPECT(bool_result);
    EXPECT(WIFEXITED(status));
    EXPECT(WEXITSTATUS(status) == 0);
}

int main(void)
{
    struct p101_error *err;
    struct p101_env   *env;
    int                exit_status;

    exit_status = EXIT_FAILURE;
    err         = p101_error_create(false);
    if(err != NULL)
    {
        env = p101_env_create(err, NULL);
        if(env != NULL)
        {
            test_tool_run(env, err);
            if(failures == 0)
            {
                exit_status = EXIT_SUCCESS;
            }
            p101_env_destroy(env);
        }
        p101_error_destroy(err);
    }

    return exit_status;
}
