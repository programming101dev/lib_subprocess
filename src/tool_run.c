#include <errno.h>
#include <fcntl.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_env/wrapper.h>
#include <p101_io/p101_aio.h>
#include <p101_io/p101_fcntl.h>
#include <p101_io/p101_poll.h>
#include <p101_io/p101_stdio.h>
#include <p101_io/p101_unistd.h>
#include <p101_io/sys/p101_select.h>
#include <p101_io/sys/p101_uio.h>
#include <p101_ipc/p101_unistd.h>
#include <p101_ipc/sys/p101_ipc.h>
#include <p101_ipc/sys/p101_mman.h>
#include <p101_ipc/sys/p101_msg.h>
#include <p101_ipc/sys/p101_sem.h>
#include <p101_ipc/sys/p101_shm.h>
#include <p101_ipc/sys/p101_stat.h>
#include <p101_process/p101_sched.h>
#include <p101_process/p101_setjmp.h>
#include <p101_process/p101_signal.h>
#include <p101_process/p101_spawn.h>
#include <p101_process/p101_stdio.h>
#include <p101_process/p101_stdlib.h>
#include <p101_process/p101_unistd.h>
#include <p101_process/sys/p101_resource.h>
#include <p101_process/sys/p101_times.h>
#include <p101_process/sys/p101_wait.h>
#include <p101_subprocess/tool_run.h>
#include <spawn.h>
#include <stdio.h>
#include <unistd.h>

#ifdef __APPLE__
    #include <crt_externs.h>
#endif

#ifdef __FreeBSD__
extern char **environ;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

enum
{
    P101_TOOL_RUN_EXEC_FAILURE = 127,
    P101_TOOL_RUN_MESSAGE_LEN  = 512,
    P101_TOOL_ARGV_INITIAL     = 8
};

static _Noreturn void p101_tool_run_child_main(const struct p101_env *env, struct p101_error *err, char *const argv[], const struct p101_tool_run_options *options, const char *diagnostic_name) P101_ATTR_SEMANTIC_ROLE("p101:termination-adapter");
static bool           tool_argv_reserve(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *arguments);
static char         **tool_environment(void);

static char **tool_environment(void)
{
    char **environment;

#ifdef __APPLE__
    char ***environment_address;

    environment_address = _NSGetEnviron();
    environment         = *environment_address;
#else
    environment = environ;
#endif
    return environment;
}

void p101_tool_argv_init(struct p101_tool_argv *arguments)
{
    arguments->values   = NULL;
    arguments->count    = 0U;
    arguments->capacity = 0U;
}

static bool tool_argv_reserve(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *arguments)
{
    bool   p101_single_result_;
    size_t new_capacity;
    char **new_values;
    void  *new_storage;

    if(arguments->count + 1U < arguments->capacity)
    {
        p101_single_result_ = true;
        goto p101_single_exit_;
    }

    new_capacity = arguments->capacity == 0U ? P101_TOOL_ARGV_INITIAL : arguments->capacity * 2U;
    if(new_capacity <= arguments->capacity || new_capacity > SIZE_MAX / sizeof(*arguments->values))
    {
        P101_ERROR_RAISE_ERRNO(err, ERANGE);
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    new_storage = p101_realloc(env, err, (void *)arguments->values, new_capacity * sizeof(*arguments->values));
    new_values  = (char **)new_storage;
    if(new_values == NULL)
    {
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    arguments->values   = new_values;
    arguments->capacity = new_capacity;
    p101_single_result_ = true;
    goto p101_single_exit_;

p101_single_exit_:
    return p101_single_result_;
}

bool p101_tool_argv_append(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *arguments, const char *value)
{
    bool   p101_single_result_;
    char  *copy;
    bool   result;
    bool   reserved;
    size_t length;
    void  *storage;

    P101_TRACE_SCOPE(env);
    P101_WRAPPER_FAULT_SCOPE_RETURN(env, err, result, false);
    length  = p101_strlen(env, value);
    storage = p101_malloc(env, err, length + 1U);
    copy    = (char *)storage;
    if(copy == NULL)
    {
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    p101_memcpy(env, copy, value, length + 1U);
    reserved = tool_argv_reserve(env, err, arguments);
    if(!reserved)
    {
        p101_free(env, copy);
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    arguments->values[arguments->count++] = copy;
    arguments->values[arguments->count]   = NULL;
    result                                = true;

    P101_WRAPPER_SCOPE_DONE();
    p101_single_result_ = result;
    goto p101_single_exit_;

p101_single_exit_:
    return p101_single_result_;
}

bool p101_tool_argv_append_prefixed(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *arguments, const char *prefix, const char *value)
{
    bool   p101_single_result_;
    char  *copy;
    bool   result;
    bool   reserved;
    size_t prefix_length;
    size_t value_length;
    void  *storage;

    P101_TRACE_SCOPE(env);
    P101_WRAPPER_FAULT_SCOPE_RETURN(env, err, result, false);
    prefix_length = p101_strlen(env, prefix);
    value_length  = p101_strlen(env, value);
    if(prefix_length > SIZE_MAX - value_length - 1U)
    {
        P101_ERROR_RAISE_ERRNO(err, ERANGE);
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    storage = p101_malloc(env, err, prefix_length + value_length + 1U);
    copy    = (char *)storage;
    if(copy == NULL)
    {
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    p101_memcpy(env, copy, prefix, prefix_length);
    p101_memcpy(env, copy + prefix_length, value, value_length + 1U);
    reserved = tool_argv_reserve(env, err, arguments);
    if(!reserved)
    {
        p101_free(env, copy);
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    arguments->values[arguments->count++] = copy;
    arguments->values[arguments->count]   = NULL;
    result                                = true;

    P101_WRAPPER_SCOPE_DONE();
    p101_single_result_ = result;
    goto p101_single_exit_;

p101_single_exit_:
    return p101_single_result_;
}

void p101_tool_argv_destroy(const struct p101_env *env, struct p101_tool_argv *arguments)
{
    P101_TRACE_SCOPE(env);
    if(arguments != NULL)
    {
        for(size_t index = 0U; index < arguments->count; index++)
        {
            p101_free(env, arguments->values[index]);
        }
        p101_free(env, (void *)arguments->values);
        p101_tool_argv_init(arguments);
    }
}

int p101_tool_run_capture(const struct p101_env *env, struct p101_error *err, char *const argv[], const struct p101_tool_run_options *options)
{
    const char *diagnostic_name;
    int         status;
    pid_t       pid;
    bool        error_present;

    P101_TRACE_SCOPE(env);
    P101_WRAPPER_FAULT_SCOPE_RETURN(env, err, status, -1);
    status          = 0;
    diagnostic_name = (options->diagnostic_name == NULL) ? "p101 tool" : options->diagnostic_name;
    p101_fflush(env, err, stdout);
    error_present = p101_error_has_error(err);
    if(!error_present)
    {
        p101_fflush(env, err, stderr);
    }
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        goto done;
    }
    pid           = p101_fork(env, err);
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        goto done;
    }

    if(pid == 0)
    {
        p101_tool_run_child_main(env, err, argv, options, diagnostic_name);
    }

    p101_waitpid(env, err, pid, &status, 0);

done:
    P101_WRAPPER_SCOPE_DONE();
    return status;
}

static _Noreturn void p101_tool_run_child_main(const struct p101_env *env, struct p101_error *err, char *const argv[], const struct p101_tool_run_options *options, const char *diagnostic_name)
{
    char        message[P101_TOOL_RUN_MESSAGE_LEN];
    const char *operation_name;
    const char *error_message;
    bool        error_present;

    operation_name = "child setup";
    p101_tool_run_redirect(env, err, options->stdout_path, options->stderr_path, options->output_mode);
    if(options->child_setup != NULL)
    {
        options->child_setup(env, err, options->child_setup_context);
    }
    error_present = p101_error_has_error(err);
    if(!error_present)
    {
        operation_name = "exec";
        p101_execvp(env, err, argv[0], argv);
    }
    error_message = p101_error_get_message(err);
    p101_strncpy(env, message, error_message, sizeof(message) - 1U);
    message[sizeof(message) - 1U] = '\0';
    p101_error_reset(err);
    p101_fprintf(env, err, stderr, "%s: %s failed: %s\n", diagnostic_name, operation_name, message);
    p101_env_complete_event_streams(env);
    /*
     * This function is the entry point of the process created by fork().
     * Returning would resume the parent's workflow in the child, so this is
     * the child process's main boundary and the sole non-application-main
     * termination exception.
     */
    _exit(P101_TOOL_RUN_EXEC_FAILURE);
}

void p101_tool_run_redirect(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path, mode_t output_mode)
{
    int  stdout_fd;
    int  stderr_fd;
    bool error_present;

    P101_TRACE_SCOPE(env);
    P101_WRAPPER_FAULT_SCOPE_RETURN_VOID(env, err);
    stdout_fd     = p101_open(env, err, stdout_path, O_WRONLY | O_CREAT | O_TRUNC, output_mode);
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        goto done;
    }

    stderr_fd     = p101_open(env, err, stderr_path, O_WRONLY | O_CREAT | O_TRUNC, output_mode);
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        p101_close(env, err, stdout_fd);
        goto done;
    }

    p101_dup2(env, err, stdout_fd, STDOUT_FILENO);
    p101_dup2(env, err, stderr_fd, STDERR_FILENO);
    p101_close(env, err, stdout_fd);
    p101_close(env, err, stderr_fd);

done:
    P101_WRAPPER_SCOPE_DONE();
}

bool p101_tool_read_pipe_open(const struct p101_env *env, struct p101_error *err, char *const argv[], const char *diagnostic_name, bool merge_stderr, struct p101_tool_read_pipe *pipe_state)
{
    bool                       p101_single_result_;
    int                        descriptors[2];
    pid_t                      pid;
    posix_spawn_file_actions_t file_actions;
    bool                       result;
    bool                       error_present;
    char                     **environment;
    int                        destroy_status;
    pid_t                      waited_pid;

    P101_TRACE_SCOPE(env);
    P101_WRAPPER_FAULT_SCOPE_RETURN(env, err, result, false);
    (void)diagnostic_name;
    pipe_state->stream = NULL;
    pipe_state->pid    = -1;
    p101_pipe(env, err, descriptors);
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    p101_posix_spawn_file_actions_init(env, err, &file_actions);
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[0]);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the fork failure.
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[1]);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the fork failure.
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    p101_posix_spawn_file_actions_addclose(env, err, &file_actions, descriptors[0]);
    if(descriptors[1] != STDOUT_FILENO)
    {
        p101_posix_spawn_file_actions_adddup2(env, err, &file_actions, descriptors[1], STDOUT_FILENO);
    }
    if(merge_stderr && descriptors[1] != STDERR_FILENO)
    {
        p101_posix_spawn_file_actions_adddup2(env, err, &file_actions, descriptors[1], STDERR_FILENO);
    }
    if(descriptors[1] != STDOUT_FILENO && (!merge_stderr || descriptors[1] != STDERR_FILENO))
    {
        p101_posix_spawn_file_actions_addclose(env, err, &file_actions, descriptors[1]);
    }
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        destroy_status = p101_posix_spawn_file_actions_destroy(env, P101_ERROR_OPTIONAL, &file_actions);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the file-action failure.
        (void)destroy_status;
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[0]);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the close failure.
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[1]);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the file-action failure.
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    environment = tool_environment();
    p101_posix_spawnp(env, err, &pid, argv[0], &file_actions, NULL, argv, environment);
    destroy_status = p101_posix_spawn_file_actions_destroy(env, P101_ERROR_OPTIONAL, &file_actions);    // P101_ERROR_OPTIONAL rationale: the spawn result is authoritative.
    (void)destroy_status;
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[0]);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the spawn failure.
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[1]);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the spawn failure.
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    p101_close(env, err, descriptors[1]);
    error_present = p101_error_has_error(err);
    if(error_present)
    {
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[0]);                 // P101_ERROR_OPTIONAL rationale: cleanup preserves the close failure.
        waited_pid = p101_waitpid(env, P101_ERROR_OPTIONAL, pid, NULL, 0);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the close failure.
        (void)waited_pid;
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    pipe_state->stream = p101_fdopen(env, err, descriptors[0], "r");
    if(pipe_state->stream == NULL)
    {
        p101_close(env, P101_ERROR_OPTIONAL, descriptors[0]);                 // P101_ERROR_OPTIONAL rationale: cleanup preserves the fdopen failure.
        waited_pid = p101_waitpid(env, P101_ERROR_OPTIONAL, pid, NULL, 0);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the fdopen failure.
        (void)waited_pid;
        p101_single_result_ = false;
        goto p101_single_exit_;
    }
    pipe_state->pid = pid;
    result          = true;

    P101_WRAPPER_SCOPE_DONE();
    p101_single_result_ = result;
    goto p101_single_exit_;

p101_single_exit_:
    return p101_single_result_;
}

int p101_tool_read_pipe_close(const struct p101_env *env, struct p101_error *err, struct p101_tool_read_pipe *pipe_state)
{
    int   status;
    bool  error_present;
    pid_t waited_pid;

    P101_TRACE_SCOPE(env);
    P101_WRAPPER_FAULT_SCOPE_RETURN(env, err, status, -1);
    status = 0;
    if(pipe_state->stream != NULL)
    {
        p101_fclose(env, err, pipe_state->stream);
        pipe_state->stream = NULL;
    }
    if(pipe_state->pid >= 0)
    {
        struct p101_error *wait_error;

        /*
         * Always reap the child, but do not overwrite a stream-close error
         * with a later wait failure.
         */
        wait_error    = err;
        error_present = p101_error_has_error(err);
        if(error_present)
        {
            wait_error = NULL;
        }
        waited_pid = p101_waitpid(env, wait_error, pipe_state->pid, &status, 0);
        (void)waited_pid;
        pipe_state->pid = -1;
    }
    P101_WRAPPER_SCOPE_DONE();
    return status;
}
