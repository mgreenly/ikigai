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

    // Build success JSON using yyjson
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", true);

    yyjson_mut_val *data = yyjson_mut_obj(doc);
    if (data == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    yyjson_mut_obj_add_str(doc, data, "output", output);
    yyjson_mut_obj_add_int(doc, data, "exit_code", exit_code); // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_val(doc, root, "data", data);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Copy to talloc
    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(result);
}
