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
            char *error_json = talloc_asprintf(parent,
                                               "{\"success\": false, \"error\": \"Failed to write file: %s\"}", path);
            if (error_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            return OK(error_json);
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

    // Build success JSON
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

    // Build output message
    char *output_msg = talloc_asprintf(parent, "Wrote %zu bytes to %s", bytes_written, filename);
    if (output_msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_str(doc, data, "output", output_msg);
    yyjson_mut_obj_add_uint(doc, data, "bytes", bytes_written);
    yyjson_mut_obj_add_val(doc, root, "data", data);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(result);
}
