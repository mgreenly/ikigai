#include "tool.h"
#include "tool_response.h"

#include "panic.h"

#include <assert.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Glob result data for response callback
typedef struct {
    const char *output;
    size_t count;
} glob_result_data_t;

// Callback to add glob-specific fields to data object
static void add_glob_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    glob_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_uint(doc, data, "count", d->count);
}

res_t ik_tool_exec_glob(void *parent, const char *pattern, const char *path)
{
    assert(pattern != NULL); // LCOV_EXCL_BR_LINE

    // Build full pattern: path/pattern or just pattern
    char *full_pattern;
    if (path != NULL && path[0] != '\0') {
        full_pattern = talloc_asprintf(parent, "%s/%s", path, pattern);
    } else {
        full_pattern = talloc_strdup(parent, pattern);
    }
    if (full_pattern == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Execute glob
    glob_t glob_result;
    int glob_ret = glob(full_pattern, 0, NULL, &glob_result);

    // Handle glob errors
    // LCOV_EXCL_START - Error paths are difficult to test (GLOB_NOSPACE, GLOB_ABORTED)
    if (glob_ret != 0 && glob_ret != GLOB_NOMATCH) {
        const char *error_msg;
        switch (glob_ret) {
            case GLOB_NOSPACE:
                error_msg = "Out of memory during glob";
                break;
            case GLOB_ABORTED:
                error_msg = "Read error during glob";
                break;
            default:
                error_msg = "Invalid glob pattern";
                break;
        }

        // Build error JSON
        char *result;
        ik_tool_response_error(parent, error_msg, &result);
        globfree(&glob_result);
        return OK(result);
    }
    // LCOV_EXCL_STOP

    // Build output string with newline-separated file paths
    size_t count = glob_result.gl_pathc;
    char *output;

    if (count == 0) {
        output = talloc_strdup(parent, "");
    } else {
        // Calculate total size needed
        size_t total_size = 0;
        for (size_t i = 0; i < count; i++) {
            total_size += strlen(glob_result.gl_pathv[i]);
            if (i < count - 1) {
                total_size += 1; // for newline
            }
        }
        total_size += 1; // for null terminator

        // Allocate and build output string
        output = talloc_array(parent, char, (unsigned int)total_size);
        if (output == NULL) { // LCOV_EXCL_BR_LINE
            globfree(&glob_result); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        output[0] = '\0';
        for (size_t i = 0; i < count; i++) {
            strcat(output, glob_result.gl_pathv[i]);
            if (i < count - 1) {
                strcat(output, "\n");
            }
        }
    }

    if (output == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build success response with data object
    glob_result_data_t result_data = {
        .output = output,
        .count = count
    };
    char *result;
    ik_tool_response_success_with_data(parent, add_glob_data, &result_data, &result);
    globfree(&glob_result);
    return OK(result);
}
