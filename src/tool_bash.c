#include "tool.h"
#include "tool_response.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <talloc.h>

// Bash result data for response callback
typedef struct {
    const char *output;
    int exit_code;
} bash_result_data_t;

// Callback to add bash-specific fields to data object
static void add_bash_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    bash_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_int(doc, data, "exit_code", d->exit_code);
}

res_t ik_tool_exec_bash(void *parent, const char *command)
{
    assert(command != NULL); // LCOV_EXCL_BR_LINE

    // Execute command with popen
    FILE *fp = popen_(command, "r");
    if (fp == NULL) {
        // popen failed - return error envelope
        char *result;
        ik_tool_response_error(parent, "Failed to execute command", &result);
        return OK(result);
    }

    // Read output from command
    // Use a dynamic buffer to capture all output
    size_t buffer_size = 4096;
    size_t total_read = 0;
    char *output = talloc_array(parent, char, (unsigned int)buffer_size);
    if (output == NULL) { // LCOV_EXCL_BR_LINE
        pclose_(fp); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    output[0] = '\0';

    char chunk[1024];
    size_t bytes_read;
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        // Ensure we have enough space
        if (total_read + bytes_read + 1 > buffer_size) {
            buffer_size = (total_read + bytes_read + 1) * 2;
            output = talloc_realloc(parent, output, char, (unsigned int)buffer_size);
            if (output == NULL) { // LCOV_EXCL_BR_LINE
                pclose_(fp); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            }
        }

        memcpy(output + total_read, chunk, bytes_read);
        total_read += bytes_read;
        output[total_read] = '\0';
    }

    // Remove trailing newline if present
    if (total_read > 0 && output[total_read - 1] == '\n') {
        output[total_read - 1] = '\0';
        total_read--;
    }

    // Get exit code
    int status = pclose_(fp);
    int exit_code;
    if (status == -1) {
        // pclose failed - treat as exit code 127
        exit_code = 127;
    } else {
        // Extract actual exit code from status
        exit_code = WEXITSTATUS(status);
    }

    // Build success response with data object
    bash_result_data_t result_data = {
        .output = output,
        .exit_code = exit_code
    };
    char *result;
    ik_tool_response_success_with_data(parent, add_bash_data, &result_data, &result);
    return OK(result);
}
