#include <errno.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_subprocess/tool_run.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;

struct fault_state
{
    int checks;
    int code;
};

#define EXPECT(condition)                                                                                                                                                                                                                                          \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        if(!(condition))                                                                                                                                                                                                                                           \
        {                                                                                                                                                                                                                                                          \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                                                                                                                                                             \
            failures++;                                                                                                                                                                                                                                            \
        }                                                                                                                                                                                                                                                          \
    } while(0)

static int fail_call(const struct p101_env *env, const char *call_name, void *user_data)
{
    struct fault_state *state;

    (void)env;
    (void)call_name;
    state = user_data;
    state->checks++;
    return state->code;
}

static void test_options(struct p101_tool_run_options *options, const char *stdout_path, const char *stderr_path)
{
    options->stdout_path         = stdout_path;
    options->stderr_path         = stderr_path;
    options->diagnostic_name     = "test tool";
    options->output_mode         = 0600;
    options->child_setup         = NULL;
    options->child_setup_context = NULL;
}

static void test_boundary_clean(struct p101_env *env, struct p101_error *err) P101_ATTR_SEMANTIC_ROLE("p101:boundary-case:boundary:subprocess-execution:clean")
{
    struct p101_tool_run_options options;
    char                        *arguments[] = {"/usr/bin/true", NULL};
    bool                         no_error;
    int                          status;

    test_options(&options, "/dev/null", "/dev/null");
    status   = p101_tool_run_capture(env, err, arguments, &options);
    no_error = p101_error_has_no_error(err);
    EXPECT(no_error);
    EXPECT(WIFEXITED(status));
    EXPECT(WEXITSTATUS(status) == 0);
}

static void test_boundary_typed_refusal(struct p101_env *env, struct p101_error *err) P101_ATTR_SEMANTIC_ROLE("p101:boundary-case:boundary:subprocess-execution:typed_refusal")
{
    struct p101_tool_run_options options;
    struct fault_state           state       = {0, EINVAL};
    char                        *arguments[] = {"/usr/bin/true", NULL};
    bool                         expected_error;
    int                          status;

    test_options(&options, "/dev/null", "/dev/null");
    p101_env_set_fault_injector(env, fail_call, &state);
    status         = p101_tool_run_capture(env, err, arguments, &options);
    expected_error = p101_error_is_errno(err, EINVAL);
    EXPECT(status == -1);
    EXPECT(expected_error);
    EXPECT(state.checks == 1);
    p101_env_set_fault_injector(env, NULL, NULL);
    p101_error_reset(err);
}

static void test_boundary_binding_swap(struct p101_env *env, struct p101_error *err) P101_ATTR_SEMANTIC_ROLE("p101:boundary-case:boundary:subprocess-execution:binding_swap")
{
    struct p101_tool_run_options options;
    char                         first_path[256];
    char                         second_path[256];
    char                         line[32];
    char                        *arguments[] = {"/bin/sh", "-c", "printf 'out\\n'; printf 'err\\n' >&2", NULL};
    FILE                        *stream;
    char                        *read_result;
    int                          comparison;
    int                          first_written;
    int                          second_written;
    int                          status;
    int                          remove_status;
    pid_t                        process_id;

    process_id     = getpid();
    first_written  = snprintf(first_path, sizeof(first_path), "/tmp/p101-subprocess-first-%ld.txt", (long)process_id);
    second_written = snprintf(second_path, sizeof(second_path), "/tmp/p101-subprocess-second-%ld.txt", (long)process_id);
    EXPECT(first_written > 0 && (size_t)first_written < sizeof(first_path));
    EXPECT(second_written > 0 && (size_t)second_written < sizeof(second_path));

    test_options(&options, first_path, second_path);
    status = p101_tool_run_capture(env, err, arguments, &options);
    EXPECT(WIFEXITED(status));
    EXPECT(WEXITSTATUS(status) == 0);

    test_options(&options, second_path, first_path);
    status = p101_tool_run_capture(env, err, arguments, &options);
    EXPECT(WIFEXITED(status));
    EXPECT(WEXITSTATUS(status) == 0);

    stream = fopen(first_path, "r");
    EXPECT(stream != NULL);
    if(stream != NULL)
    {
        read_result = fgets(line, sizeof(line), stream);
        EXPECT(read_result != NULL);
        comparison = strcmp(line, "err\n");
        EXPECT(comparison == 0);
        status = fclose(stream);
        EXPECT(status == 0);
    }
    stream = fopen(second_path, "r");
    EXPECT(stream != NULL);
    if(stream != NULL)
    {
        read_result = fgets(line, sizeof(line), stream);
        EXPECT(read_result != NULL);
        comparison = strcmp(line, "out\n");
        EXPECT(comparison == 0);
        status = fclose(stream);
        EXPECT(status == 0);
    }
    remove_status = remove(first_path);
    EXPECT(remove_status == 0);
    remove_status = remove(second_path);
    EXPECT(remove_status == 0);
}

static void test_boundary_identity_mismatch(struct p101_env *env, struct p101_error *err) P101_ATTR_SEMANTIC_ROLE("p101:boundary-case:boundary:subprocess-execution:identity_mismatch")
{
    struct p101_tool_run_options options;
    char                        *arguments[] = {"/definitely/missing/p101-tool", NULL};
    bool                         no_error;
    int                          status;

    test_options(&options, "/dev/null", "/dev/null");
    status   = p101_tool_run_capture(env, err, arguments, &options);
    no_error = p101_error_has_no_error(err);
    EXPECT(no_error);
    EXPECT(WIFEXITED(status));
    EXPECT(WEXITSTATUS(status) == 127);
}

static void test_boundary_resource_limit(struct p101_env *env, struct p101_error *err) P101_ATTR_SEMANTIC_ROLE("p101:boundary-case:boundary:subprocess-execution:resource_limit")
{
    struct p101_tool_run_options options;
    struct fault_state           state       = {0, ENOMEM};
    char                        *arguments[] = {"/usr/bin/true", NULL};
    bool                         expected_error;
    int                          status;

    test_options(&options, "/dev/null", "/dev/null");
    p101_env_set_fault_injector(env, fail_call, &state);
    status         = p101_tool_run_capture(env, err, arguments, &options);
    expected_error = p101_error_is_errno(err, ENOMEM);
    EXPECT(status == -1);
    EXPECT(expected_error);
    EXPECT(state.checks == 1);
    p101_env_set_fault_injector(env, NULL, NULL);
    p101_error_reset(err);
}

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
            test_boundary_clean(env, err);
            test_boundary_typed_refusal(env, err);
            test_boundary_binding_swap(env, err);
            test_boundary_identity_mismatch(env, err);
            test_boundary_resource_limit(env, err);
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
