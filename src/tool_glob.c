#include "tool.h"
#include "tool_response.h"

#include "panic.h"

#include <assert.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

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

    // Create yyjson document for result
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        globfree(&glob_result); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

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
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
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

    // Build success JSON using yyjson
    yyjson_mut_obj_add_bool(doc, root, "success", true);

    yyjson_mut_val *data = yyjson_mut_obj(doc);
    if (data == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        globfree(&glob_result); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    yyjson_mut_obj_add_str(doc, data, "output", output);
    yyjson_mut_obj_add_uint(doc, data, "count", count);
    yyjson_mut_obj_add_val(doc, root, "data", data);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        globfree(&glob_result); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Copy to talloc
    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);
    globfree(&glob_result);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(result);
}
