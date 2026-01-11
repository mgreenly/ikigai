#include "tool.h"
#include "tool_response.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// File write result data for response callback
typedef struct {
    const char *output;
    size_t bytes;
} file_write_result_data_t;

// Callback to add file_write-specific fields to data object
static void add_file_write_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    file_write_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str_(doc, data, "output", d->output);
    yyjson_mut_obj_add_uint_(doc, data, "bytes", d->bytes);
}

res_t ik_tool_exec_file_write(void *parent, const char *path, const char *content)
{
    assert(path != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL); // LCOV_EXCL_BR_LINE

    // Open file for writing (creates file if doesn't exist, truncates if exists)
    FILE *f = fopen_(path, "w");
    if (f == NULL) {
        // Determine error message based on errno
        char *error_msg;
        if (errno == EACCES) {
            error_msg = talloc_asprintf(parent, "Permission denied: %s", path);
        } else if (errno == ENOSPC) {
            error_msg = talloc_asprintf(parent, "No space left on device: %s", path);
        } else {
            error_msg = talloc_asprintf(parent, "Cannot open file: %s", path);
        }
        if (error_msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        // Build error JSON
        char *result;
        ik_tool_response_error(parent, error_msg, &result);
        return OK(result);
    }

    // Write content to file
    size_t content_len = strlen(content);
    size_t bytes_written = 0;

    if (content_len > 0) {
        bytes_written = fwrite_(content, 1, content_len, f);
        if (bytes_written != content_len) {
            fclose_(f);
            char *error_msg = talloc_asprintf(parent, "Failed to write file: %s", path);
            if (error_msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            char *result;
            ik_tool_response_error(parent, error_msg, &result);
            return OK(result);
        }
    }

    fclose_(f);

    // Extract just the filename from the path for the message
    char *path_copy = talloc_strdup(parent, path);
    if (path_copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    char *filename_ptr = basename(path_copy);

    // Copy basename result since it might be in static storage
    char *filename = talloc_strdup(parent, filename_ptr);
    if (filename == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    talloc_free(path_copy);

    // Build output message
    char *output_msg = talloc_asprintf(parent, "Wrote %zu bytes to %s", bytes_written, filename);
    if (output_msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build success response with data
    file_write_result_data_t result_data = {
        .output = output_msg,
        .bytes = bytes_written
    };
    char *result;
    ik_tool_response_success_with_data(parent, add_file_write_data, &result_data, &result);
    return OK(result);
}
