#include <fcntl.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_subprocess/tool_run.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* P101_TEST_CASE(p101_tool_run_redirect) */
int main(void)
{
    struct p101_error *err;
    struct p101_env   *env;
    char               stdout_path[128];
    char               stderr_path[128];
    struct stat        info;
    pid_t              child;
    int                status;

    if(snprintf(stdout_path, sizeof(stdout_path), "/tmp/p101-util-stdout-%ld", (long)getpid()) < 0 || snprintf(stderr_path, sizeof(stderr_path), "/tmp/p101-util-stderr-%ld", (long)getpid()) < 0)
    {
        return EXIT_FAILURE;
    }
    err = p101_error_create(false);
    if(err == NULL)
    {
        return EXIT_FAILURE;
    }
    env = p101_env_create(err, NULL);
    if(env == NULL)
    {
        p101_error_destroy(err);
        return EXIT_FAILURE;
    }

    child = fork();
    if(child == 0)
    {
        p101_tool_run_redirect(env, err, stdout_path, stderr_path, 0600);
        if(p101_error_has_error(err))
        {
            p101_env_complete_event_streams(env);
            _exit(2);
        }
        if(write(STDOUT_FILENO, "out", 3) != 3 || write(STDERR_FILENO, "err", 3) != 3)
        {
            p101_env_complete_event_streams(env);
            _exit(3);
        }
        p101_env_complete_event_streams(env);
        _exit(0);
    }
    if(child < 0 || waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        p101_env_destroy(env);
        p101_error_destroy(err);
        unlink(stdout_path);
        unlink(stderr_path);
        return EXIT_FAILURE;
    }

    status = stat(stdout_path, &info);
    if(status == 0 && info.st_size == 3)
    {
        status = stat(stderr_path, &info);
    }
    p101_env_destroy(env);
    p101_error_destroy(err);
    unlink(stdout_path);
    unlink(stderr_path);
    return status == 0 && info.st_size == 3 ? EXIT_SUCCESS : EXIT_FAILURE;
}
