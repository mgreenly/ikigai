#include "tool_external.h"

#include "error.h"
#include "panic.h"

#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

res_t ik_tool_external_exec(TALLOC_CTX *ctx,
                             const char *tool_path,
                             const char *arguments_json,
                             char **out_result)
{
    int32_t stdin_pipe[2];
    int32_t stdout_pipe[2];

    if (pipe(stdin_pipe) == -1) {
        return ERR(ctx, IO, "Failed to create stdin pipe");
    }

    if (pipe(stdout_pipe) == -1) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return ERR(ctx, IO, "Failed to create stdout pipe");
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return ERR(ctx, IO, "Failed to fork process");
    }

    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        // Redirect stderr to /dev/null
        int32_t null_fd = open("/dev/null", O_WRONLY);
        if (null_fd != -1) {
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        execl(tool_path, tool_path, NULL);
        _exit(1);
    }

    // Parent process
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Write arguments to stdin
    size_t args_len = strlen(arguments_json);
    ssize_t written = write(stdin_pipe[1], arguments_json, args_len);
    close(stdin_pipe[1]);

    if (written != (ssize_t)args_len) {
        close(stdout_pipe[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return ERR(ctx, IO, "Failed to write arguments to tool");
    }

    // Set 30 second timeout
    alarm(30);

    // Read output
    char buffer[65536];
    ssize_t total_read = 0;
    ssize_t n;

    while ((n = read(stdout_pipe[0], buffer + total_read, sizeof(buffer) - (size_t)total_read - 1)) > 0) {
        total_read += n;
        if (total_read >= (ssize_t)(sizeof(buffer) - 1)) {
            break;
        }
    }

    alarm(0);
    close(stdout_pipe[0]);

    // Wait for child
    int32_t status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return ERR(ctx, IO, "Tool exited with non-zero status");
    }

    if (total_read == 0) {
        return ERR(ctx, IO, "Tool produced no output");
    }

    buffer[total_read] = '\0';

    char *result = talloc_strdup(ctx, buffer);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    *out_result = result;
    return OK(NULL);
}
