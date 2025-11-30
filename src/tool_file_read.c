#include "tool.h"

#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

res_t ik_tool_exec_file_read(void *parent, const char *path)
{
    assert(path != NULL); // LCOV_EXCL_BR_LINE

    // Open file for reading
    FILE *f = fopen_(path, "r");
    if (f == NULL) {
        // Build error JSON
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        yyjson_mut_val *root = yyjson_mut_obj(doc);
        if (root == NULL) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }
        yyjson_mut_doc_set_root(doc, root);

        yyjson_mut_obj_add_bool(doc, root, "success", false);

        // Determine error message based on errno
        if (errno == ENOENT) {
            char *msg = talloc_asprintf(parent, "File not found: %s", path);
            if (msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_str(doc, root, "error", msg);
        } else if (errno == EACCES) {
            char *msg = talloc_asprintf(parent, "Permission denied: %s", path);
            if (msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_str(doc, root, "error", msg);
        } else {
            char *msg = talloc_asprintf(parent, "Cannot open file: %s", path);
            if (msg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_str(doc, root, "error", msg);
        }

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

    // Get file size
    if (fseek_(f, 0, SEEK_END) != 0) {
        fclose_(f);
        char *error_json = talloc_asprintf(parent,
                                           "{\"success\": false, \"error\": \"Cannot seek file: %s\"}", path);
        if (error_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return OK(error_json);
    }

    long size = ftell_(f);
    if (size < 0) {
        fclose_(f);
        char *error_json = talloc_asprintf(parent,
                                           "{\"success\": false, \"error\": \"Cannot get file size: %s\"}", path);
        if (error_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return OK(error_json);
    }

    if (fseek_(f, 0, SEEK_SET) != 0) {
        fclose_(f);
        char *error_json = talloc_asprintf(parent,
                                           "{\"success\": false, \"error\": \"Cannot rewind file: %s\"}", path);
        if (error_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return OK(error_json);
    }

    // Allocate buffer for file contents
    size_t file_size = (size_t)size;
    char *buffer = talloc_array(parent, char, (unsigned int)(file_size + 1));
    if (buffer == NULL) { // LCOV_EXCL_BR_LINE
        fclose_(f); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Read file contents
    size_t bytes_read = fread_(buffer, 1, file_size, f);
    fclose_(f);

    if (bytes_read != file_size) {
        char *error_json = talloc_asprintf(parent,
                                           "{\"success\": false, \"error\": \"Failed to read file: %s\"}", path);
        if (error_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return OK(error_json);
    }

    buffer[file_size] = '\0';

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

    yyjson_mut_obj_add_str(doc, data, "output", buffer);
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
