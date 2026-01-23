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
                            const char *agent_id,
                            const char *arguments_json,
                            char **out_result)
{
    int32_t stdin_pipe[2];
    int32_t stdout_pipe[2];
    int32_t stderr_pipe[2];

    if (pipe(stdin_pipe) == -1) {  // LCOV_EXCL_BR_LINE
        return ERR(ctx, IO, "Failed to create stdin pipe");  // LCOV_EXCL_LINE
    }

    if (pipe(stdout_pipe) == -1) {  // LCOV_EXCL_BR_LINE
        close(stdin_pipe[0]);  // LCOV_EXCL_LINE
        close(stdin_pipe[1]);  // LCOV_EXCL_LINE
        return ERR(ctx, IO, "Failed to create stdout pipe");  // LCOV_EXCL_LINE
    }

    if (pipe(stderr_pipe) == -1) {  // LCOV_EXCL_BR_LINE
        close(stdin_pipe[0]);  // LCOV_EXCL_LINE
        close(stdin_pipe[1]);  // LCOV_EXCL_LINE
        close(stdout_pipe[0]);  // LCOV_EXCL_LINE
        close(stdout_pipe[1]);  // LCOV_EXCL_LINE
        return ERR(ctx, IO, "Failed to create stderr pipe");  // LCOV_EXCL_LINE
    }

    pid_t pid = fork();
    if (pid == -1) {  // LCOV_EXCL_BR_LINE
        close(stdin_pipe[0]);  // LCOV_EXCL_LINE
        close(stdin_pipe[1]);  // LCOV_EXCL_LINE
        close(stdout_pipe[0]);  // LCOV_EXCL_LINE
        close(stdout_pipe[1]);  // LCOV_EXCL_LINE
        close(stderr_pipe[0]);  // LCOV_EXCL_LINE
        close(stderr_pipe[1]);  // LCOV_EXCL_LINE
        return ERR(ctx, IO, "Failed to fork process");  // LCOV_EXCL_LINE
    }

    if (pid == 0) {  // LCOV_EXCL_START
        // Child process
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Set agent ID environment variable for tools that need it
        if (agent_id != NULL) {
            setenv("IKIGAI_AGENT_ID", agent_id, 1);
        }

        execl(tool_path, tool_path, NULL);
        _exit(1);
    }  // LCOV_EXCL_STOP

    // Parent process
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Write arguments to stdin
    size_t args_len = strlen(arguments_json);
    ssize_t written = write(stdin_pipe[1], arguments_json, args_len);
    close(stdin_pipe[1]);

    if (written != (ssize_t)args_len) {  // LCOV_EXCL_BR_LINE
        close(stdout_pipe[0]);  // LCOV_EXCL_LINE
        close(stderr_pipe[0]);  // LCOV_EXCL_LINE
        kill(pid, SIGKILL);  // LCOV_EXCL_LINE
        waitpid(pid, NULL, 0);  // LCOV_EXCL_LINE
        return ERR(ctx, IO, "Failed to write arguments to tool");  // LCOV_EXCL_LINE
    }

    // Set 30 second timeout
    alarm(30);

    // Read output
    char stdout_buffer[65536];
    ssize_t stdout_total = 0;
    ssize_t n;

    while ((n = read(stdout_pipe[0], stdout_buffer + stdout_total, sizeof(stdout_buffer) - (size_t)stdout_total - 1)) > 0) {
        stdout_total += n;
        if (stdout_total >= (ssize_t)(sizeof(stdout_buffer) - 1)) {
            break;
        }
    }

    close(stdout_pipe[0]);

    // Read stderr
    char stderr_buffer[65536];
    ssize_t stderr_total = 0;

    while ((n = read(stderr_pipe[0], stderr_buffer + stderr_total, sizeof(stderr_buffer) - (size_t)stderr_total - 1)) > 0) {
        stderr_total += n;
        if (stderr_total >= (ssize_t)(sizeof(stderr_buffer) - 1)) {
            break;
        }
    }

    alarm(0);
    close(stderr_pipe[0]);

    // Wait for child
    int32_t status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {  // LCOV_EXCL_BR_LINE
        if (stderr_total > 0) {
            stderr_buffer[stderr_total] = '\0';
            return ERR(ctx, IO, "Tool failed: %s", stderr_buffer);
        }
        return ERR(ctx, IO, "Tool exited with non-zero status");
    }

    if (stdout_total == 0) {
        return ERR(ctx, IO, "Tool produced no output");
    }

    stdout_buffer[stdout_total] = '\0';

    char *result = talloc_strdup(ctx, stdout_buffer);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    *out_result = result;
    return OK(NULL);
}
